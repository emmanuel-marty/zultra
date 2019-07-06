/*
 * libzultra.c - optimal zlib compression library definitions
 *
 * Copyright (C) 2019 Emmanuel Marty
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/*
 * Uses the libdivsufsort library Copyright (c) 2003-2008 Yuta Mori
 * Uses the xxhash implementation by Stephan Brumme. https://create.stephan-brumme.com/xxhash/
 *
 * Inspired by zlib by Jean-loup Gailly and Mark Adler. https://github.com/madler/zlib
 * Also inspired by Zopfli by Lode Vandevenne and Jyrki Alakuijala. https://github.com/google/zopfli
 * With ideas from libdeflate by Eric Biggers. https://github.com/ebiggers/libdeflate
 * Also with ideas from linzip2 by Gary Linscott. https://glinscott.github.io/lz/index.html
 *
 */

#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "blockdeflate.h"
#include "matchfinder.h"
#include "format.h"
#include "private.h"
#include "libzultra.h"

/* Current compression state */
#define  CSTATE_HAS_DICTIONARY          1    /**< Dictionary set */
#define  CSTATE_HEADER_EMITTED          2    /**< Compressed bitstream header emitted */
#define  CSTATE_FINALIZED_COMPRESSION   4    /**< All blocks compressed, ready to emit footer */
#define  CSTATE_FOOTER_EMITTED          8    /**< Compressed bitstream footer emitted */

/**
 * Default memory allocator
 *
 * @param opaque object passed to memory functions
 * @param items number of items to allocate
 * @param size size of one item
 *
 * @return starting address of allocated memory, or NULL for failure
 */
static void *zultra_default_zalloc(void *opaque, unsigned int items, unsigned int size) {
   return malloc(items * size);
}

/**
 * Default memory deallocator
 *
 * @param opaque object passed to memory functions
 * @param address starting address of memory to deallocate
 */
static void zultra_default_zfree(void *opaque, void *address) {
   free(address);
}

/**
 * Initialize streaming compression context
 *
 * @param pStream streaming compression context
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 * @param nMaxBlockSize maximum block size (0 for default)
 *
 * @return ZULTRA_OK for success, or an error value from zultra_status_t
 */
zultra_status_t zultra_stream_init(zultra_stream_t *pStream, const unsigned int nFlags, unsigned int nMaxBlockSize) {
   zultra_compressor_t *pCompressor;
   int nMaxWindowSize;
   int nResult;

   if (!nMaxBlockSize)
      nMaxBlockSize = ZULTRA_DEFAULT_MAX_BLOCK_SIZE;
   if (nMaxBlockSize < 32768)
      nMaxBlockSize = 32768;
   if (nMaxBlockSize > 2097152)
      nMaxBlockSize = 2097152;

   if (!pStream->zalloc)
      pStream->zalloc = zultra_default_zalloc;
   if (!pStream->zfree)
      pStream->zfree = zultra_default_zfree;

   pStream->adler = 0;
   pStream->state = pCompressor = (zultra_compressor_t*)pStream->zalloc(pStream->opaque, 1, sizeof(zultra_compressor_t));
   if (!pStream->state) {
      zultra_stream_end(pStream);
      return ZULTRA_ERROR_MEMORY;
   }

   memset(pCompressor, 0, sizeof(zultra_compressor_t));

   pCompressor->in_data = (unsigned char*)pStream->zalloc(pStream->opaque, HISTORY_SIZE + nMaxBlockSize, 1);
   if (!pCompressor->in_data) {
      zultra_stream_end(pStream);
      return ZULTRA_ERROR_MEMORY;
   }
   memset(pCompressor->in_data, 0, HISTORY_SIZE + nMaxBlockSize);

   pCompressor->out_buffer = (unsigned char*)pStream->zalloc(pStream->opaque, 1 + nMaxBlockSize + (1 + 4) * ((nMaxBlockSize / 65535) + 1), 1);
   if (!pCompressor->out_buffer) {
      zultra_stream_end(pStream);
      return ZULTRA_ERROR_MEMORY;
   }
   memset(pCompressor->out_buffer, 0, 1 + nMaxBlockSize + (1 + 4) * ((nMaxBlockSize / 65535) + 1));

   nResult = divsufsort_init(&pCompressor->divsufsort_context, pStream->zalloc, pStream->zfree, pStream->opaque);
   pCompressor->intervals = NULL;
   pCompressor->pos_data = NULL;
   pCompressor->open_intervals = NULL;
   pCompressor->match = NULL;
   pCompressor->best_match = NULL;
   pCompressor->flags = nFlags;
   pCompressor->max_block_size = nMaxBlockSize;

   nMaxWindowSize = HISTORY_SIZE + nMaxBlockSize;

   if (!nResult) {
      nResult = -1;
      pCompressor->intervals = (unsigned int *)pStream->zalloc(pStream->opaque, nMaxWindowSize, sizeof(unsigned int));

      if (pCompressor->intervals) {
         pCompressor->pos_data = (unsigned int *)pStream->zalloc(pStream->opaque, nMaxWindowSize, sizeof(unsigned int));

         if (pCompressor->pos_data) {
            pCompressor->open_intervals = (unsigned int *)pStream->zalloc(pStream->opaque, (LCP_MAX + 1), sizeof(unsigned int));

            if (pCompressor->open_intervals) {
               pCompressor->match = (zultra_match_t *)pStream->zalloc(pStream->opaque, nMaxWindowSize * NMATCHES_PER_OFFSET, sizeof(zultra_match_t));

               if (pCompressor->match) {
                  pCompressor->best_match = (zultra_match_t *)pStream->zalloc(pStream->opaque, nMaxWindowSize, sizeof(zultra_match_t));

                  if (pCompressor->best_match) {
                     nResult = 0;
                  }
               }
            }
         }
      }
   }

   if (nResult != 0) {
      zultra_stream_end(pStream);
      return ZULTRA_ERROR_MEMORY;
   }

   zultra_bitwriter_init(&pCompressor->bitwriter, pCompressor->out_buffer, 0, 1 + nMaxBlockSize + (1 + 4) * ((nMaxBlockSize / 65535) + 1));

   return ZULTRA_OK;
}

/**
 * Set dictionary to use for compression
 *
 * @param pStream streaming compression context
 * @param pDictionaryData dictionary contents
 * @param nDictionaryDataSize size of dictionary contents, in bytes
 *
 * @return ZULTRA_OK for success, or an error value from zultra_status_t
 */
zultra_status_t zultra_stream_set_dictionary(zultra_stream_t *pStream, const void *pDictionaryData, const int nDictionaryDataSize) {
   zultra_compressor_t *pCompressor = pStream->state;

   if (pCompressor && pCompressor->compression_state == 0) {
      pCompressor->dictionary_data = pDictionaryData;
      pCompressor->dictionary_size = nDictionaryDataSize;
      pCompressor->compression_state |= CSTATE_HAS_DICTIONARY;
      
      return ZULTRA_OK;
   }
   else {
      return ZULTRA_ERROR_COMPRESSION;
   }
}

/**
 * Compress data
 *
 * @param pStream streaming compression context
 * @param nDoFinalize ZULTRA_FINALIZE if the input data is the last to be compressed, ZULTRA_CONTINUE if more data is coming
 *
 * @return ZULTRA_OK for success, or an error value from zultra_status_t
 */
zultra_status_t zultra_stream_compress(zultra_stream_t *pStream, const int nDoFinalize) {
   zultra_compressor_t *pCompressor = pStream->state;
   zultra_status_t nError = ZULTRA_OK;

   do {
      const int nMaxBlockSize = pCompressor->max_block_size;
      const int nMaxSplits = MAX_SPLITS;

      if ((pCompressor->compression_state & CSTATE_HEADER_EMITTED) == 0) {
         pCompressor->compression_state |= CSTATE_HEADER_EMITTED;

         int nHeaderSize = zultra_frame_encode_header(pCompressor->frame_buffer, 16, pCompressor->flags, pCompressor->dictionary_data, pCompressor->dictionary_size);
         if (nHeaderSize < 0)
            nError = ZULTRA_ERROR_COMPRESSION;
         else {
            pCompressor->cur_frame_index = 0;
            pCompressor->pending_frame_bytes = nHeaderSize;
         }

         pStream->adler = zultra_frame_init_checksum(pCompressor->flags);
      }

      if (!nError && pCompressor->pending_frame_bytes) {
         if ((pCompressor->cur_frame_index + pCompressor->pending_frame_bytes) > 16)
            nError = ZULTRA_ERROR_COMPRESSION;
         else {
            if (pStream->avail_out) {
               size_t nMaxFrameOutBytes = pStream->avail_out;

               if (nMaxFrameOutBytes > pCompressor->pending_frame_bytes)
                  nMaxFrameOutBytes = pCompressor->pending_frame_bytes;
               memcpy(pStream->next_out, pCompressor->frame_buffer + pCompressor->cur_frame_index, nMaxFrameOutBytes);

               pCompressor->cur_frame_index += nMaxFrameOutBytes;
               pCompressor->pending_frame_bytes -= nMaxFrameOutBytes;

               pStream->next_out += nMaxFrameOutBytes;
               pStream->avail_out -= nMaxFrameOutBytes;
               pStream->total_out += (long long)nMaxFrameOutBytes;
            }
         }
      }

      if (!nError && !pCompressor->previous_block_size && pCompressor->dictionary_size && pCompressor->dictionary_data) {
         memcpy(pCompressor->in_data + HISTORY_SIZE - pCompressor->dictionary_size, pCompressor->dictionary_data, pCompressor->dictionary_size);
         pCompressor->previous_block_size = pCompressor->dictionary_size;
      }

      if (!nError && !pCompressor->pending_frame_bytes && !pCompressor->pending_out_bytes) {
         size_t nMaxInBytes = pStream->avail_in;

         if (nMaxInBytes > (nMaxBlockSize - pCompressor->cur_in_bytes))
            nMaxInBytes = nMaxBlockSize - pCompressor->cur_in_bytes;
         memcpy(pCompressor->in_data + HISTORY_SIZE + pCompressor->cur_in_bytes, pStream->next_in, nMaxInBytes);

         pStream->next_in += nMaxInBytes;
         pStream->avail_in -= nMaxInBytes;
         pStream->total_in += (long long)nMaxInBytes;

         pCompressor->cur_in_bytes += nMaxInBytes;

         if ((pCompressor->cur_in_bytes >= nMaxBlockSize && pStream->avail_in) || nDoFinalize) {
            int nInDataSize;

            nInDataSize = (int)pCompressor->cur_in_bytes;

            if (nInDataSize > 0) {
               zultra_bitwriter_t blockBitWriter;

               pStream->adler = zultra_frame_update_checksum(pStream->adler, pCompressor->in_data + HISTORY_SIZE, nInDataSize, pCompressor->flags);

               pCompressor->dictionary_size = 0;
               pCompressor->cur_in_bytes = 0;

               int nCompressionResult;

               if (zultra_build_suffix_array(pStream->state, pCompressor->in_data + HISTORY_SIZE - pCompressor->previous_block_size, pCompressor->previous_block_size + nInDataSize))
                  nError = ZULTRA_ERROR_COMPRESSION;
               else {
                  if (pCompressor->previous_block_size) {
                     zultra_skip_matches(pStream->state, 0, pCompressor->previous_block_size);
                  }
                  zultra_find_all_matches(pStream->state, pCompressor->previous_block_size, pCompressor->previous_block_size + nInDataSize);
               }

               if (!nError) {
                  int nInStart = 0;
                  int nNumSplits = 0;
                  int nSplitOffset[MAX_SPLITS];
                  int nSplitArraySize;

                  nSplitArraySize = zultra_block_split(pStream->state, pCompressor->in_data + HISTORY_SIZE - pCompressor->previous_block_size, pCompressor->previous_block_size + nInStart, nInDataSize - nInStart,
                     nMaxSplits, nSplitOffset);
                  if (nSplitArraySize < 0)
                     nError = ZULTRA_ERROR_COMPRESSION;
                  else {
                     while (nInStart < nInDataSize && !nError) {
                        int nBlockSize, nStaticCost = 0, nDynamicCost = 0;
                        int nIsFinal;
                        int nIsDynamic = 1;

                        nBlockSize = nSplitOffset[nNumSplits++] - (nInStart + pCompressor->previous_block_size);

                        if (zultra_block_prepare_cost_evaluation(pStream->state, pCompressor->in_data + HISTORY_SIZE - pCompressor->previous_block_size, pCompressor->previous_block_size + nInStart, nBlockSize) < 0 ||
                           zultra_block_evaluate_static_cost(&pStream->state->literalsEncoder, &pStream->state->offsetEncoder, &nStaticCost) < 0 ||
                           zultra_huffman_encoder_estimate_dynamic_codelens(&pStream->state->literalsEncoder) < 0 ||
                           zultra_huffman_encoder_estimate_dynamic_codelens(&pStream->state->offsetEncoder) < 0 ||
                           zultra_block_evaluate_dynamic_cost(&pStream->state->literalsEncoder, &pStream->state->offsetEncoder, &nDynamicCost) < 0)
                           nError = ZULTRA_ERROR_COMPRESSION;
                        if (nStaticCost <= nDynamicCost)
                           nIsDynamic = 0;

                        zultra_bitwriter_copy(&blockBitWriter, &pCompressor->bitwriter);
                        nIsFinal = (nDoFinalize && (nInStart + nBlockSize) >= nInDataSize && !pStream->avail_in) ? 1 : 0;
                        if (zultra_bitwriter_put_bits(&pCompressor->bitwriter, nIsFinal /* last block */, 1) < 0)
                           nError = ZULTRA_ERROR_DST;
                        else {
                           if (zultra_bitwriter_put_bits(&pCompressor->bitwriter, 1 + nIsDynamic /* compressed with static or dynamic Huffman codes */, 2) < 0)
                              nError = ZULTRA_ERROR_DST;
                        }

                        if (!nError) {
                           int nPrevOffset = zultra_bitwriter_get_offset(&pCompressor->bitwriter);

                           if (nPrevOffset < 0)
                              nCompressionResult = ZULTRA_ERROR_DST;
                           else
                              nCompressionResult = zultra_block_deflate(pStream->state, &pCompressor->bitwriter, pCompressor->in_data + HISTORY_SIZE - pCompressor->previous_block_size, pCompressor->previous_block_size + nInStart, nBlockSize, nIsDynamic);

                           if (nCompressionResult < 0 ||
                              zultra_bitwriter_get_offset(&pCompressor->bitwriter) < 0 ||
                              (zultra_bitwriter_get_offset(&pCompressor->bitwriter) - nPrevOffset) > nBlockSize) {

                              /* Not compressible; rewind to final block bit */
                              zultra_bitwriter_copy(&pCompressor->bitwriter, &blockBitWriter);

                              int nSubBlockOffset = 0;
                              int nRemainingBlockSize = nBlockSize;
                              while (!nError && nRemainingBlockSize) {
                                 int nSubBlockSize = nRemainingBlockSize;
                                 int nSubIsFinal = nIsFinal;

                                 if (nSubBlockSize > 65535) {
                                    /* Larger than max size for stored blocks */
                                    nSubBlockSize = 65535;
                                    nSubIsFinal = 0;
                                 }

                                 /* Write last block bit */
                                 if (zultra_bitwriter_put_bits(&pCompressor->bitwriter, nSubIsFinal /* last block */, 1) < 0)
                                    nError = ZULTRA_ERROR_DST;
                                 else {
                                    /* Write block type */
                                    if (zultra_bitwriter_put_bits(&pCompressor->bitwriter, 0 /* no compression */, 2) < 0)
                                       nError = ZULTRA_ERROR_DST;
                                 }

                                 if (!nError) {
                                    /* Pad bits to an integral number of bytes */
                                    if (zultra_bitwriter_flush_bits(&pCompressor->bitwriter) < 0)
                                       nError = ZULTRA_ERROR_DST;
                                 }

                                 if (!nError) {
                                    int nCurWriteOffset = zultra_bitwriter_get_offset(&pCompressor->bitwriter);

                                    if (nCurWriteOffset < 0 || (nCurWriteOffset + 4 + nSubBlockSize) >(1 + nMaxBlockSize + (1 + 4) * ((nMaxBlockSize / 65535) + 1)))
                                       nError = ZULTRA_ERROR_DST;
                                    else {
                                       pCompressor->out_buffer[nCurWriteOffset++] = nSubBlockSize & 0xff;
                                       pCompressor->out_buffer[nCurWriteOffset++] = (nSubBlockSize >> 8) & 0xff;
                                       pCompressor->out_buffer[nCurWriteOffset++] = (nSubBlockSize & 0xff) ^ 0xff;
                                       pCompressor->out_buffer[nCurWriteOffset++] = ((nSubBlockSize >> 8) & 0xff) ^ 0xff;
                                       memcpy(pCompressor->out_buffer + nCurWriteOffset, pCompressor->in_data + HISTORY_SIZE + nInStart + nSubBlockOffset, nSubBlockSize);
                                       nCurWriteOffset += nSubBlockSize;
                                       zultra_bitwriter_set_offset(&pCompressor->bitwriter, nCurWriteOffset);

                                       nSubBlockOffset += nSubBlockSize;
                                       nRemainingBlockSize -= nSubBlockSize;
                                    }
                                 }
                              }
                           }

                           nInStart += nBlockSize;

                        }
                     }
                  }
               }

               pCompressor->previous_block_size = nInDataSize;
               if (pCompressor->previous_block_size > HISTORY_SIZE)
                  pCompressor->previous_block_size = HISTORY_SIZE;

               if (pCompressor->previous_block_size) {
                  memcpy(pCompressor->in_data + HISTORY_SIZE - pCompressor->previous_block_size, pCompressor->in_data + HISTORY_SIZE + (nMaxBlockSize - pCompressor->previous_block_size), pCompressor->previous_block_size);
               }

               if (!nError && nDoFinalize && !pStream->avail_in) {
                  /* Pad bits to an integral number of bytes */
                  if (zultra_bitwriter_flush_bits(&pCompressor->bitwriter) < 0)
                     nError = ZULTRA_ERROR_DST;
                  else {
                     pCompressor->compression_state |= CSTATE_FINALIZED_COMPRESSION;
                  }
               }

               if (!nError) {
                  /* Ready compressed block(s) to be output */

                  int nCurWriteOffset = zultra_bitwriter_get_offset(&pCompressor->bitwriter);
                  if (nCurWriteOffset < 0)
                     nError = ZULTRA_ERROR_DST;
                  else {
                     pCompressor->cur_out_index = 0;
                     pCompressor->pending_out_bytes = nCurWriteOffset;

                     zultra_bitwriter_set_offset(&pCompressor->bitwriter, 0);
                  }
               }
            }
         }
      }

      if (!nError && !pCompressor->pending_frame_bytes && pCompressor->pending_out_bytes) {
         size_t nMaxOutSize = 1 + nMaxBlockSize + (1 + 4) * ((nMaxBlockSize / 65535) + 1);
         if ((pCompressor->cur_out_index + pCompressor->pending_frame_bytes) > nMaxOutSize)
            nError = ZULTRA_ERROR_COMPRESSION;
         else {
            if (pStream->avail_out) {
               size_t nMaxOutDataBytes = pStream->avail_out;

               if (nMaxOutDataBytes > pCompressor->pending_out_bytes)
                  nMaxOutDataBytes = pCompressor->pending_out_bytes;
               memcpy(pStream->next_out, pCompressor->out_buffer + pCompressor->cur_out_index, nMaxOutDataBytes);

               pCompressor->cur_out_index += nMaxOutDataBytes;
               pCompressor->pending_out_bytes -= nMaxOutDataBytes;

               pStream->next_out += nMaxOutDataBytes;
               pStream->avail_out -= nMaxOutDataBytes;
               pStream->total_out += (long long)nMaxOutDataBytes;
            }
         }
      }

      if (!nError && !pCompressor->pending_frame_bytes && !pCompressor->pending_out_bytes && (pCompressor->compression_state & CSTATE_FINALIZED_COMPRESSION) && !(pCompressor->compression_state & CSTATE_FOOTER_EMITTED)) {
         int nFooterSize = zultra_frame_encode_footer(pCompressor->frame_buffer, 16, pStream->adler, pStream->total_in, pCompressor->flags);

         if (nFooterSize < 0)
            nError = ZULTRA_ERROR_COMPRESSION;
         else {
            pCompressor->compression_state = (pCompressor->compression_state | CSTATE_FOOTER_EMITTED) & (~CSTATE_FINALIZED_COMPRESSION);
            pCompressor->cur_frame_index = 0;
            pCompressor->pending_frame_bytes = nFooterSize;
         }
      }

      if (!nError && pCompressor->pending_frame_bytes) {
         if ((pCompressor->cur_frame_index + pCompressor->pending_frame_bytes) > 16)
            nError = ZULTRA_ERROR_COMPRESSION;
         else {
            if (pStream->avail_out) {
               size_t nMaxFrameOutBytes = pStream->avail_out;

               if (nMaxFrameOutBytes > pCompressor->pending_frame_bytes)
                  nMaxFrameOutBytes = pCompressor->pending_frame_bytes;
               memcpy(pStream->next_out, pCompressor->frame_buffer + pCompressor->cur_frame_index, nMaxFrameOutBytes);

               pCompressor->cur_frame_index += nMaxFrameOutBytes;
               pCompressor->pending_frame_bytes -= nMaxFrameOutBytes;

               pStream->next_out += nMaxFrameOutBytes;
               pStream->avail_out -= nMaxFrameOutBytes;
               pStream->total_out += (long long)nMaxFrameOutBytes;
            }
         }
      }
   } while (!nError && pStream->avail_in && pStream->avail_out);

   return nError;
}

/**
 * Destroy streaming compression context
 *
 * @param pStream streaming compression context
 */
void zultra_stream_end(zultra_stream_t *pStream) {
   if (pStream->state && pStream->zfree) {
      zultra_compressor_t *pCompressor = pStream->state;

      divsufsort_destroy(&pCompressor->divsufsort_context, pStream->zfree, pStream->opaque);

      if (pCompressor->best_match) {
         pStream->zfree(pStream->opaque, pCompressor->best_match);
         pCompressor->best_match = NULL;
      }

      if (pCompressor->match) {
         pStream->zfree(pStream->opaque, pCompressor->match);
         pCompressor->match = NULL;
      }

      if (pCompressor->open_intervals) {
         pStream->zfree(pStream->opaque, pCompressor->open_intervals);
         pCompressor->open_intervals = NULL;
      }

      if (pCompressor->pos_data) {
         pStream->zfree(pStream->opaque, pCompressor->pos_data);
         pCompressor->pos_data = NULL;
      }

      if (pCompressor->intervals) {
         pStream->zfree(pStream->opaque, pCompressor->intervals);
         pCompressor->intervals = NULL;
      }

      if (pCompressor->out_buffer) {
         pStream->zfree(pStream->opaque, pCompressor->out_buffer);
         pCompressor->out_buffer = NULL;
      }

      if (pCompressor->in_data) {
         pStream->zfree(pStream->opaque, pCompressor->in_data);
         pCompressor->in_data = NULL;
      }

      pStream->zfree(pStream->opaque, pStream->state);
      pStream->state = NULL;
   }
}

/**
 * Get maximum compressed size of input(source) data
 *
 * @param nInputSize input(source) size in bytes
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 * @param nMaxBlockSize maximum block size (0 for default)
 *
 * @return maximum compressed size
 */
size_t zultra_memory_bound(size_t nInputSize, const unsigned int nFlags, unsigned int nMaxBlockSize) {
   const int nMaxSplits = MAX_SPLITS;

   if (!nMaxBlockSize)
      nMaxBlockSize = ZULTRA_DEFAULT_MAX_BLOCK_SIZE;
   if (nMaxBlockSize < 32768)
      nMaxBlockSize = 32768;
   if (nMaxBlockSize > 2097152)
      nMaxBlockSize = 2097152;

   return zultra_frame_get_header_size(nFlags, NULL, 0) + ((nInputSize + (nMaxBlockSize - 1)) / nMaxBlockSize) * (1 + 4 + 1) * nMaxSplits + nInputSize + 1 + zultra_frame_get_footer_size(nFlags);
}

/**
 * Compress memory
 *
 * @param pInputData pointer to input(source) data to compress
 * @param pOutBuffer buffer for compressed data
 * @param nInputSize input(source) size in bytes
 * @param nMaxOutBufferSize maximum capacity of compression buffer
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 * @param nMaxBlockSize maximum block size (0 for default)
 *
 * @return actual compressed size, or -1 for error
 */
size_t zultra_memory_compress(const unsigned char *pInputData, size_t nInputSize, unsigned char *pOutBuffer, size_t nMaxOutBufferSize, const unsigned int nFlags, unsigned int nMaxBlockSize) {
   zultra_stream_t strm;
   zultra_status_t nStatus;

   memset(&strm, 0, sizeof(strm));

   nStatus = zultra_stream_init(&strm, nFlags, nMaxBlockSize);
   if (nStatus) return -1;

   strm.next_in = pInputData;
   strm.avail_in = nInputSize;
   strm.next_out = pOutBuffer;
   strm.avail_out = nMaxOutBufferSize;
   nStatus = zultra_stream_compress(&strm, ZULTRA_FINALIZE);
   zultra_stream_end(&strm);

   if (nStatus) return -1;
   return nMaxOutBufferSize - strm.avail_out;
}
