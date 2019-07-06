/*
 * lzwide.c - command line optimal compression utility for the lzwide format
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif
#include "libzultra.h"
#include "zlib.h"    /* decompression only, for do_compare() and do_self_test() */

#define OPT_VERBOSE        1
#define OPT_FORMAT_DEFLATE 2
#define OPT_FORMAT_ZLIB    4
#define OPT_FORMAT_GZIP    8
#define OPT_FORMAT_MASK    14

#define TOOL_VERSION "1.0.0"

/*---------------------------------------------------------------------------*/

#ifdef _WIN32
LARGE_INTEGER hpc_frequency;
BOOL hpc_available = FALSE;
#endif

static void do_init_time() {
#ifdef _WIN32
   hpc_frequency.QuadPart = 0;
   hpc_available = QueryPerformanceFrequency(&hpc_frequency);
#endif
}

static long long do_get_time() {
   long long nTime;

#ifdef _WIN32
   if (hpc_available) {
      LARGE_INTEGER nCurTime;

      /* Use HPC hardware for best precision */
      QueryPerformanceCounter(&nCurTime);
      nTime = (long long)(nCurTime.QuadPart * 1000000LL / hpc_frequency.QuadPart);
   }
   else {
      struct _timeb tb;
      _ftime(&tb);

      nTime = ((long long)tb.time * 1000LL + (long long)tb.millitm) * 1000LL;
   }
#else
   struct timeval tm;
   gettimeofday(&tm, NULL);

   nTime = (long long)tm.tv_sec * 1000000LL + (long long)tm.tv_usec;
#endif
   return nTime;
}

/*---------------------------------------------------------------------------*/

int do_compress(const char *pszInFilename, const char *pszOutFilename, const char *pszDictionaryFilename, const unsigned int nOptions) {
   const size_t CHUNK_SIZE = 16384;
   FILE *inStream = NULL, *outStream = NULL;
   unsigned char *pInBuffer = NULL, *pOutBuffer = NULL;
   void *pDictionaryData = NULL;
   int nDictionaryDataSize = 0;
   zultra_status_t nStatus = ZULTRA_OK;
   int flush = 0;
   zultra_stream_t strm;
   unsigned int nFlags = 0;
   long long nStartTime = 0LL, nEndTime = 0LL;

   memset(&strm, 0, sizeof(strm));

   if (nOptions & OPT_VERBOSE) {
      nStartTime = do_get_time();
   }

   if (nOptions & OPT_FORMAT_ZLIB) {
      nFlags |= ZULTRA_FLAG_ZLIB_FRAMING;
   }
   else if (nOptions & OPT_FORMAT_GZIP) {
      nFlags |= ZULTRA_FLAG_GZIP_FRAMING;
   }

   inStream = fopen(pszInFilename, "rb");
   if (!inStream) {
      nStatus = ZULTRA_ERROR_SRC;
   }

   if (!nStatus) {
      outStream = fopen(pszOutFilename, "wb");
      if (!outStream) {
         nStatus = ZULTRA_ERROR_DST;
      }
   }

   if (!nStatus) {
      nStatus = zultra_dictionary_load(pszDictionaryFilename, &pDictionaryData, &nDictionaryDataSize);
   }

   if (!nStatus) {
      pInBuffer = (unsigned char *)malloc(CHUNK_SIZE);
      if (!pInBuffer)
         nStatus = ZULTRA_ERROR_MEMORY;
   }

   if (!nStatus) {
      pOutBuffer = (unsigned char *)malloc(CHUNK_SIZE);
      if (!pOutBuffer)
         nStatus = ZULTRA_ERROR_MEMORY;
   }

   if (!nStatus) {
      nStatus = zultra_stream_init(&strm, nFlags, 0);
   }

   if (!nStatus && pDictionaryData) {
      nStatus = zultra_stream_set_dictionary(&strm, pDictionaryData, nDictionaryDataSize);
   }

   while (!flush && !nStatus) {
      int nHasProgress = 0;

      strm.avail_in = fread(pInBuffer, 1, CHUNK_SIZE, inStream);
      if (ferror(inStream)) {
         zultra_stream_end(&strm);
         nStatus = ZULTRA_ERROR_SRC;
         break;
      }
      flush = feof(inStream) ? ZULTRA_FINALIZE : ZULTRA_CONTINUE;
      strm.next_in = pInBuffer;

      do {
         size_t nOutBytesToWrite;

         strm.avail_out = CHUNK_SIZE;
         strm.next_out = pOutBuffer;
         nStatus = zultra_stream_compress(&strm, flush);
         if (nStatus != ZULTRA_OK) break;

         nOutBytesToWrite = CHUNK_SIZE - strm.avail_out;
         if (nOutBytesToWrite) nHasProgress = 1;

         if (fwrite(pOutBuffer, 1, nOutBytesToWrite, outStream) != nOutBytesToWrite || ferror(outStream)) {
            zultra_stream_end(&strm);
            nStatus = ZULTRA_ERROR_DST;
            break;
         }
      } while (strm.avail_out == 0);

      if (!nStatus && strm.avail_in != 0)
         nStatus = ZULTRA_ERROR_COMPRESSION;

      if (!nStatus && !flush && nHasProgress && strm.total_in && strm.total_out >= 1024) {
         fprintf(stdout, "\r%lld => %lld (%g %%)     \b\b\b\b\b", strm.total_in, strm.total_out, (double)(strm.total_out * 100.0 / strm.total_in));
         fflush(stdout);
      }
   }

   zultra_stream_end(&strm);

   if (pOutBuffer)
      free(pOutBuffer);
   if (pInBuffer)
      free(pInBuffer);

   zultra_dictionary_free(&pDictionaryData);
   if (outStream)
      fclose(outStream);
   if (inStream)
      fclose(inStream);

   if (nStatus) {
      switch (nStatus) {
      case ZULTRA_ERROR_SRC: fprintf(stderr, "error reading '%s'\n", pszInFilename); break;
      case ZULTRA_ERROR_DST: fprintf(stderr, "error writing '%s'\n", pszOutFilename); break;
      case ZULTRA_ERROR_DICTIONARY: fprintf(stderr, "error reading dictionary '%s'\n", pszDictionaryFilename); break;
      case ZULTRA_ERROR_MEMORY: fprintf(stderr, "'%s': out of memory\n", pszInFilename); break;
      case ZULTRA_ERROR_COMPRESSION: fprintf(stderr, "'%s': internal compression error\n", pszInFilename); break;
      case ZULTRA_OK: break;
      default: fprintf(stderr, "unknown compression error %d\n", nStatus); break;
      }

      return 100;
   }
   else {
      if ((nOptions & OPT_VERBOSE) && strm.total_in && strm.total_out) {
         nEndTime = do_get_time();

         double fDelta = ((double)(nEndTime - nStartTime)) / 1000000.0;
         double fSpeed = ((double)strm.total_in / 1048576.0) / fDelta;
         fprintf(stdout, "\rCompressed '%s' in %g seconds, %.02g Mb/s, %lld into %lld bytes ==> %g %%\n",
            pszInFilename, fDelta, fSpeed,
            strm.total_in, strm.total_out, strm.total_in ? (double)(strm.total_out * 100.0 / strm.total_in) : 100.0);
      }

      return 0;
   }
}

/*---------------------------------------------------------------------------*/

static int do_compare(const char *pszInFilename, const char *pszOutFilename, const char *pszDictionaryFilename, const unsigned int nOptions) {
   FILE *inStream, *compareStream;
   size_t nInBufferMaxSize = 16384;
   size_t nOutBufferMaxSize = 65536;
   unsigned char *pInBuffer, *pOutBuffer, *pCompareBuffer;
   zultra_status_t nStatus;
   void *pDictionaryData = NULL;
   int nDictionaryDataSize = 0;

   inStream = fopen(pszInFilename, "rb");
   if (!inStream) {
      fprintf(stderr, "error opening compressed input file\n");
      return 100;
   }

   compareStream = fopen(pszOutFilename, "rb");
   if (!compareStream) {
      fprintf(stderr, "error opening original uncompressed file\n");
      fclose(inStream);
      return 100;
   }

   nStatus = zultra_dictionary_load(pszDictionaryFilename, &pDictionaryData, &nDictionaryDataSize);
   if (nStatus) {
      fclose(compareStream);
      fclose(inStream);

      return nStatus;
   }

   pInBuffer = (unsigned char *)malloc(nInBufferMaxSize);
   if (!pInBuffer) {
      fprintf(stderr, "out of memory\n");
      zultra_dictionary_free(&pDictionaryData);
      fclose(compareStream);
      fclose(inStream);
      return 100;
   }

   pOutBuffer = (unsigned char *)malloc(nOutBufferMaxSize);
   if (!pOutBuffer) {
      fprintf(stderr, "out of memory\n");
      zultra_dictionary_free(&pDictionaryData);
      free(pInBuffer);
      fclose(compareStream);
      fclose(inStream);
      return 100;
   }

   pCompareBuffer = (unsigned char *)malloc(nOutBufferMaxSize);
   if (!pCompareBuffer) {
      fprintf(stderr, "out of memory\n");
      zultra_dictionary_free(&pDictionaryData);
      free(pOutBuffer);
      free(pInBuffer);
      fclose(compareStream);
      fclose(inStream);
      return 100;
   }

   z_stream strm = { 0 };
   int nZlibResult;
   int nDecompressionError;

   strm.avail_in = 0;
   strm.next_in = NULL;
   strm.avail_out = 0;
   strm.next_out = NULL;
   strm.zalloc = NULL;
   strm.zfree = NULL;
   strm.opaque = 0;

   if (nOptions & OPT_FORMAT_ZLIB)
      nZlibResult = inflateInit2(&strm, (15 + 32));
   else if (nOptions & OPT_FORMAT_GZIP)
      nZlibResult = inflateInit2(&strm, (15 + 16));
   else
      nZlibResult = inflateInit2(&strm, -15);
   if (nZlibResult != Z_OK) {
      fprintf(stderr, "error initializing decompression\n");
      zultra_dictionary_free(&pDictionaryData);
      free(pCompareBuffer);
      free(pOutBuffer);
      free(pInBuffer);
      fclose(compareStream);
      fclose(inStream);
      return 100;
   }

   nDecompressionError = 0;
   while (!nDecompressionError) {
      if (strm.avail_in == 0) {
         strm.next_in = (unsigned char *)pInBuffer;
         size_t nInBytes = fread(pInBuffer, 1, nInBufferMaxSize, inStream);

         strm.avail_in += (uInt)nInBytes;
         if (nInBytes == 0)
            break;
      }

      if (strm.avail_in != 0) {
         strm.next_out = (Bytef*)pOutBuffer;
         strm.avail_out = (uInt)nOutBufferMaxSize;

         nZlibResult = inflate(&strm, 0);
         if (nZlibResult == Z_NEED_DICT) {
            if (pDictionaryData && nDictionaryDataSize) {
               inflateSetDictionary(&strm, pDictionaryData, nDictionaryDataSize);
               continue;
            }
            else {
               fprintf(stderr, "dictionary required to compare '%s'\n", pszInFilename);
               nDecompressionError = 1;
               break;
            }
         }

         if (nZlibResult < 0 ) {
            fprintf(stderr, "decompression error %d for '%s'\n", nZlibResult, pszInFilename);
            nDecompressionError = 1;
            break;
         }

         if (strm.total_out != 0) {
            size_t nCompareBytes = fread(pCompareBuffer, 1, strm.total_out, compareStream);

            if (nCompareBytes != strm.total_out) {
               fprintf(stderr, "error reading back '%s' for comparison\n", pszOutFilename);
               nDecompressionError = 1;
               break;
            }

            if (feof(compareStream)) {
               fprintf(stderr, "error, finished decompressing but there is still more data in '%s'\n", pszOutFilename);
               nDecompressionError = 1;
               break;
            }

            if (memcmp(pCompareBuffer, pOutBuffer, strm.total_out)) {
               fprintf(stderr, "error comparing '%s' with the decompressed contents of '%s'\n", pszInFilename, pszOutFilename);
               nDecompressionError = 1;
               break;
            }

            strm.total_out = 0;
         }

         if (strm.avail_in == 0 && nZlibResult != Z_STREAM_END && feof(inStream)) {
            fprintf(stderr, "error, finished reading '%s' but decompression didn't finish\n", pszInFilename);
            nDecompressionError = 1;
            break;
         }
      }
   }

   if (!nDecompressionError && nZlibResult != Z_STREAM_END) {
      fprintf(stderr, "error, finished comparing '%s' but decompression didn't finish\n", pszInFilename);
      nDecompressionError = 1;
   }

   inflateEnd(&strm);

   zultra_dictionary_free(&pDictionaryData);
   free(pCompareBuffer);
   free(pOutBuffer);
   free(pInBuffer);

   fclose(compareStream);
   fclose(inStream);

   if (nDecompressionError) {
      return 100;
   }
   else {
      if (nOptions & OPT_VERBOSE) {
         fprintf(stdout, "Compared '%s' OK\n", pszOutFilename);
      }

      return 0;
   }
}

/*---------------------------------------------------------------------------*/

static void generate_compressible_data(unsigned char *pBuffer, size_t nBufferSize, unsigned int nSeed, int nNumLiteralValues, float fMatchProbability) {
   size_t nIndex = 0;
   int nMatchProbability = (int)(fMatchProbability * 1023.0f);

   srand(nSeed);
   
   if (nIndex >= nBufferSize) return;
   pBuffer[nIndex++] = rand() % nNumLiteralValues;

   while (nIndex < nBufferSize) {
      if ((rand() & 1023) >= nMatchProbability) {
         size_t nLiteralCount = rand() & 127;
         if (nLiteralCount > (nBufferSize - nIndex))
            nLiteralCount = nBufferSize - nIndex;

         while (nLiteralCount--)
            pBuffer[nIndex++] = rand() % nNumLiteralValues;
      }
      else {
         size_t nMatchLength = MIN_MATCH_SIZE + (rand() & 1023);
         size_t nMatchOffset;

         if (nMatchLength > (nBufferSize - nIndex))
            nMatchLength = nBufferSize - nIndex;
         if (nMatchLength > nIndex)
            nMatchLength = nIndex;

         if (nMatchLength < nIndex)
            nMatchOffset = rand() % (nIndex - nMatchLength);
         else
            nMatchOffset = 0;

         while (nMatchLength--) {
            pBuffer[nIndex] = pBuffer[nIndex - nMatchOffset];
            nIndex++;
         }
      }
   }
}

static int do_self_test(int nIsQuickTest) {
   unsigned char *pGeneratedData;
   unsigned char *pCompressedData;
   unsigned char *pTmpCompressedData;
   unsigned char *pTmpDecompressedData;
   size_t nGeneratedDataSize;
   size_t nMaxCompressedDataSize;
   const unsigned int nFlags = ZULTRA_FLAG_ZLIB_FRAMING;
   unsigned int nSeed = 123;
   int i;

   pGeneratedData = (unsigned char*)malloc(4 * HISTORY_SIZE);
   if (!pGeneratedData) {
      fprintf(stderr, "out of memory, %d bytes needed\n", 4 * HISTORY_SIZE);
      return 100;
   }

   nMaxCompressedDataSize = zultra_memory_bound(4 * HISTORY_SIZE, nFlags, 0);
   pCompressedData = (unsigned char*)malloc(nMaxCompressedDataSize);
   if (!pCompressedData) {
      free(pGeneratedData);
      pGeneratedData = NULL;

      fprintf(stderr, "out of memory, %zd bytes needed\n", nMaxCompressedDataSize);
      return 100;
   }

   pTmpCompressedData = (unsigned char*)malloc(nMaxCompressedDataSize);
   if (!pTmpCompressedData) {
      free(pCompressedData);
      pCompressedData = NULL;
      free(pGeneratedData);
      pGeneratedData = NULL;

      fprintf(stderr, "out of memory, %zd bytes needed\n", nMaxCompressedDataSize);
      return 100;
   }

   pTmpDecompressedData = (unsigned char*)malloc(4 * HISTORY_SIZE);
   if (!pTmpDecompressedData) {
      free(pTmpCompressedData);
      pTmpCompressedData = NULL;
      free(pCompressedData);
      pCompressedData = NULL;
      free(pGeneratedData);
      pGeneratedData = NULL;

      fprintf(stderr, "out of memory, %d bytes needed\n", 4 * HISTORY_SIZE);
      return 100;
   }

   memset(pGeneratedData, 0, 4 * HISTORY_SIZE);
   memset(pCompressedData, 0, nMaxCompressedDataSize);
   memset(pTmpCompressedData, 0, nMaxCompressedDataSize);

   /* Test compressing with a too small buffer to do anything, expect to fail cleanly */
   for (i = 0; i < 12; i++) {
      generate_compressible_data(pGeneratedData, i, nSeed, 256, 0.5f);
      zultra_memory_compress(pGeneratedData, i, pCompressedData, i, nFlags, 0);
   }

   size_t nDataSizeStep = 128;
   float fProbabilitySizeStep = nIsQuickTest ? 0.005f : 0.0005f;

   for (nGeneratedDataSize = nIsQuickTest ? 4096 : 16384; nGeneratedDataSize <= (nIsQuickTest ? 4096 : (4 * HISTORY_SIZE)); nGeneratedDataSize += nDataSizeStep) {
      float fMatchProbability;

      fprintf(stdout, "size %zd", nGeneratedDataSize);
      for (fMatchProbability = 0; fMatchProbability <= 0.995f; fMatchProbability += fProbabilitySizeStep) {
         int nNumLiteralValues[12] = { 1, 2, 3, 15, 30, 56, 96, 137, 178, 191, 255, 256 };

         fputc('.', stdout);
         fflush(stdout);

         for (i = 0; i < 12; i++) {
            /* Generate data to compress */
            generate_compressible_data(pGeneratedData, nGeneratedDataSize, nSeed, nNumLiteralValues[i], fMatchProbability);

            /* Try to compress it, expected to succeed */
            size_t nActualCompressedSize = zultra_memory_compress(pGeneratedData, nGeneratedDataSize, pCompressedData, zultra_memory_bound(nGeneratedDataSize, nFlags, 0), nFlags, 0);
            if (nActualCompressedSize == -1 || nActualCompressedSize < (ZULTRA_HEADER_SIZE + ZULTRA_FRAME_SIZE + ZULTRA_FOOTER_SIZE)) {
               free(pTmpDecompressedData);
               pTmpDecompressedData = NULL;
               free(pTmpCompressedData);
               pTmpCompressedData = NULL;
               free(pCompressedData);
               pCompressedData = NULL;
               free(pGeneratedData);
               pGeneratedData = NULL;

               fprintf(stderr, "\nself-test: error compressing size %zd, seed %d, match probability %f, literals range %d\n", nGeneratedDataSize, nSeed, fMatchProbability, nNumLiteralValues[i]);
               return 100;
            }

            /* Try to decompress it, expected to succeed */

            size_t nActualDecompressedSize = (size_t)-1;
            z_stream strm = { 0 };
            strm.total_in = strm.avail_in = (int)nActualCompressedSize;
            strm.total_out = strm.avail_out = (int)nGeneratedDataSize;
            strm.next_in = (Bytef *)pCompressedData;
            strm.next_out = (Bytef *)pTmpDecompressedData;

            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;

            int err = -1;

            err = inflateInit2(&strm, (15 + 32));
            if (err == Z_OK) {
               err = inflate(&strm, Z_FINISH);
               if (err == Z_STREAM_END) {
                  nActualDecompressedSize = strm.total_out;
               }

               inflateEnd(&strm);
            }

            if (nActualDecompressedSize == -1) {
               free(pTmpDecompressedData);
               pTmpDecompressedData = NULL;
               free(pTmpCompressedData);
               pTmpCompressedData = NULL;
               free(pCompressedData);
               pCompressedData = NULL;
               free(pGeneratedData);
               pGeneratedData = NULL;

               fprintf(stderr, "\nself-test: error decompressing size %zd, seed %d, match probability %f, literals range %d\n", nGeneratedDataSize, nSeed, fMatchProbability, nNumLiteralValues[i]);
               return 100;
            }

            if (memcmp(pGeneratedData, pTmpDecompressedData, nGeneratedDataSize)) {
               free(pTmpDecompressedData);
               pTmpDecompressedData = NULL;
               free(pTmpCompressedData);
               pTmpCompressedData = NULL;
               free(pCompressedData);
               pCompressedData = NULL;
               free(pGeneratedData);
               pGeneratedData = NULL;

               fprintf(stderr, "\nself-test: error comparing decompressed and original data, size %zd, seed %d, match probability %f, literals range %d\n", nGeneratedDataSize, nSeed, fMatchProbability, nNumLiteralValues[i]);
               return 100;
            }
         }

         nSeed++;
      }

      fputc(10, stdout);
      fflush(stdout);

      nDataSizeStep <<= 1;
      if (nDataSizeStep > (128 * 4096))
         nDataSizeStep = 128 * 4096;
      fProbabilitySizeStep *= 1.25;
      if (fProbabilitySizeStep > (0.0005f * 4096))
         fProbabilitySizeStep = 0.0005f * 4096;
   }

   free(pTmpDecompressedData);
   pTmpDecompressedData = NULL;

   free(pTmpCompressedData);
   pTmpCompressedData = NULL;

   free(pCompressedData);
   pCompressedData = NULL;

   free(pGeneratedData);
   pGeneratedData = NULL;

   fprintf(stdout, "All tests passed.\n");
   return 0;
}

/*---------------------------------------------------------------------------*/

static int do_compr_benchmark(const char *pszInFilename, const char *pszOutFilename, const char *pszDictionaryFilename, const unsigned int nOptions) {
   size_t nFileSize, nMaxCompressedSize;
   unsigned char *pFileData;
   unsigned char *pCompressedData;
   unsigned int nFlags = 0;
   int i;

   if (nOptions & OPT_FORMAT_ZLIB) {
      nFlags |= ZULTRA_FLAG_ZLIB_FRAMING;
   }
   else if (nOptions & OPT_FORMAT_GZIP) {
      nFlags |= ZULTRA_FLAG_GZIP_FRAMING;
   }

   if (pszDictionaryFilename) {
      fprintf(stderr, "in-memory benchmarking does not support dictionaries\n");
      return 100;
   }
   
   /* Read the whole original file in memory */

   FILE *f_in = fopen(pszInFilename, "rb");
   if (!f_in) {
      fprintf(stderr, "error opening '%s' for reading\n", pszInFilename);
      return 100;
   }

   fseek(f_in, 0, SEEK_END);
   nFileSize = (size_t)ftell(f_in);
   fseek(f_in, 0, SEEK_SET);

   pFileData = (unsigned char*)malloc(nFileSize);
   if (!pFileData) {
      fclose(f_in);
      fprintf(stderr, "out of memory for reading '%s', %zd bytes needed\n", pszInFilename, nFileSize);
      return 100;
   }

   if (fread(pFileData, 1, nFileSize, f_in) != nFileSize) {
      free(pFileData);
      fclose(f_in);
      fprintf(stderr, "I/O error while reading '%s'\n", pszInFilename);
      return 100;
   }

   fclose(f_in);

   /* Allocate max compressed size */

   nMaxCompressedSize = zultra_memory_bound(nFileSize, nFlags, 0);

   pCompressedData = (unsigned char*)malloc(nMaxCompressedSize + 2048);
   if (!pCompressedData) {
      free(pFileData);
      fprintf(stderr, "out of memory for compressing '%s', %zd bytes needed\n", pszInFilename, nMaxCompressedSize);
      return 100;
   }

   memset(pCompressedData + 1024, 0, nMaxCompressedSize);

   long long nBestCompTime = -1;

   size_t nActualCompressedSize = 0;
   size_t nRightGuardPos = nMaxCompressedSize;

   for (i = 0; i < 5; i++) {
      unsigned char nGuard = 0x33 + i;
      int j;

      /* Write guard bytes around the output buffer, to help check for writes outside of it by the compressor */
      memset(pCompressedData, nGuard, 1024);
      memset(pCompressedData + 1024 + nRightGuardPos, nGuard, 1024);

      long long t0 = do_get_time();
      nActualCompressedSize = zultra_memory_compress(pFileData, nFileSize, pCompressedData + 1024, nRightGuardPos, nFlags, 0);
      long long t1 = do_get_time();
      if (nActualCompressedSize == -1) {
         free(pCompressedData);
         free(pFileData);
         fprintf(stderr, "compression error\n");
         return 100;
      }

      long long nCurDecTime = t1 - t0;
      if (nBestCompTime == -1 || nBestCompTime > nCurDecTime)
         nBestCompTime = nCurDecTime;

      /* Check guard bytes before the output buffer */
      for (j = 0; j < 1024; j++) {
         if (pCompressedData[j] != nGuard) {
            free(pCompressedData);
            free(pFileData);
            fprintf(stderr, "error, wrote outside of output buffer at %d!\n", j - 1024);
            return 100;
         }
      }

      /* Check guard bytes after the output buffer */
      for (j = 0; j < 1024; j++) {
         if (pCompressedData[1024 + nRightGuardPos + j] != nGuard) {
            free(pCompressedData);
            free(pFileData);
            fprintf(stderr, "error, wrote outside of output buffer at %d!\n", j);
            return 100;
         }
      }

      nRightGuardPos = nActualCompressedSize;
   }

   if (pszOutFilename) {
      FILE *f_out;

      /* Write whole compressed file out */

      f_out = fopen(pszOutFilename, "wb");
      if (f_out) {
         fwrite(pCompressedData + 1024, 1, nActualCompressedSize, f_out);
         fclose(f_out);
      }
   }

   free(pCompressedData);
   free(pFileData);

   fprintf(stdout, "compressed size: %zd bytes\n", nActualCompressedSize);
   fprintf(stdout, "compression time: %lld microseconds (%g Mb/s)\n", nBestCompTime, ((double)nActualCompressedSize / 1024.0) / ((double)nBestCompTime / 1000.0));

   return 0;
}

/*---------------------------------------------------------------------------*/

int main(int argc, char **argv) {
   int i;
   const char *pszInFilename = NULL;
   const char *pszOutFilename = NULL;
   const char *pszDictionaryFilename = NULL;
   bool bArgsError = false;
   bool bCommandDefined = false;
   bool bVerifyCompression = false;
   char cCommand = 'z';
   unsigned int nOptions = 0;

   for (i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "-d")) {
         if (!bCommandDefined) {
            bCommandDefined = true;
            cCommand = 'd';
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-z")) {
         if (!bCommandDefined) {
            bCommandDefined = true;
            cCommand = 'z';
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-c")) {
         if (!bVerifyCompression) {
            bVerifyCompression = true;
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-cbench")) {
         if (!bCommandDefined) {
            bCommandDefined = true;
            cCommand = 'B';
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-test")) {
         if (!bCommandDefined) {
            bCommandDefined = true;
            cCommand = 't';
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-quicktest")) {
         if (!bCommandDefined) {
            bCommandDefined = true;
            cCommand = 'T';
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-D")) {
         if (!pszDictionaryFilename && (i + 1) < argc) {
            pszDictionaryFilename = argv[i + 1];
            i++;
         }
         else
            bArgsError = true;
      }
      else if (!strncmp(argv[i], "-D", 2)) {
         if (!pszDictionaryFilename) {
            pszDictionaryFilename = argv[i] + 2;
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-v")) {
         if ((nOptions & OPT_VERBOSE) == 0) {
            nOptions |= OPT_VERBOSE;
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-deflate")) {
         if ((nOptions & OPT_FORMAT_MASK) == 0) {
            nOptions |= OPT_FORMAT_DEFLATE;
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-gzip")) {
         if ((nOptions & OPT_FORMAT_MASK) == 0) {
            nOptions |= OPT_FORMAT_GZIP;
         }
         else
            bArgsError = true;
      }
      else if (!strcmp(argv[i], "-zlib")) {
         if ((nOptions & OPT_FORMAT_MASK) == 0) {
            nOptions |= OPT_FORMAT_ZLIB;
         }
         else
            bArgsError = true;
      }
      else {
         if (!pszInFilename)
            pszInFilename = argv[i];
         else {
            if (!pszOutFilename)
               pszOutFilename = argv[i];
            else
               bArgsError = true;
         }
      }
   }

   if (!bArgsError && (cCommand == 't' || cCommand == 'T')) {
      return do_self_test((cCommand == 'T') ? 1 : 0);
   }

   if (bArgsError || !pszInFilename || !pszOutFilename) {
      fprintf(stderr, "zultra v" TOOL_VERSION " by Emmanuel Marty\n");
      fprintf(stderr, "usage: %s [-gzip] [-zlib] [-deflate] [-v] {-c|-cbench|-test} <infile> <outfile>\n", argv[0]);
      fprintf(stderr, "           -gzip: use gzip framing (default)\n");
      fprintf(stderr, "           -zlib: use zlib framing\n");
      fprintf(stderr, "        -deflate: use deflate framing (no framing)\n");
      fprintf(stderr, "              -v: be verbose\n");
      fprintf(stderr, "              -c: check resulting stream after compressing\n");
      fprintf(stderr, "         -cbench: benchmark in-memory compression\n");
      fprintf(stderr, "           -test: run automated self-tests\n");
      return 100;
   }

   do_init_time();

   if ((nOptions & OPT_FORMAT_MASK) == 0) {
      /* Default to gzip framing */
      nOptions |= OPT_FORMAT_GZIP;
   }

   if (cCommand == 'z') {
      if (pszDictionaryFilename && (nOptions & OPT_FORMAT_MASK) != OPT_FORMAT_ZLIB) {
         fprintf(stderr, "dictionaries are only supported for the zlib framing\n");
         return 100;
      }

      int nResult = do_compress(pszInFilename, pszOutFilename, pszDictionaryFilename, nOptions);
      if (nResult == 0 && bVerifyCompression) {
         nResult = do_compare(pszOutFilename, pszInFilename, pszDictionaryFilename, nOptions);
      }

      return nResult;
   }
   else if (cCommand == 'B') {
      return do_compr_benchmark(pszInFilename, pszOutFilename, pszDictionaryFilename, nOptions);
   }
   else {
      return 100;
   }
}
