/*
 * libzultra.h - optimal zlib compression library definitions
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
 * Uses the crc32 implementation by Stephan Brumme. https://create.stephan-brumme.com/crc32/
 *
 * Inspired by zlib by Jean-loup Gailly and Mark Adler. https://github.com/madler/zlib
 * Also inspired by Zopfli by Lode Vandevenne and Jyrki Alakuijala. https://github.com/google/zopfli
 * With ideas from libdeflate by Eric Biggers. https://github.com/ebiggers/libdeflate
 * Also with ideas from linzip2 by Gary Linscott. https://glinscott.github.io/lz/index.html
 *
 */

#ifndef _STATUS_H
#define _STATUS_H

#include "format.h"
#include "frame.h"
#include "dictionary.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct _zultra_compressor_s zultra_compressor_t;

/** High level compression status */
typedef enum _zultra_stream_e {
   ZULTRA_OK = 0,                          /**< Success */
   ZULTRA_ERROR_SRC,                       /**< Error reading input */
   ZULTRA_ERROR_DST,                       /**< Error reading output */
   ZULTRA_ERROR_DICTIONARY,                /**< Error reading dictionary */
   ZULTRA_ERROR_MEMORY,                    /**< Out of memory */
   ZULTRA_ERROR_COMPRESSION,               /**< Internal compression error */
} zultra_status_t;

/* Compression flags */
#define ZULTRA_FLAG_DEFLATE_FRAMING 0      /**< deflate stream only */
#define ZULTRA_FLAG_ZLIB_FRAMING    1      /**< zlib header, big-endian adler32 checksum */
#define ZULTRA_FLAG_GZIP_FRAMING    2      /**< gzip header, little-endian crc32 checksum */

/** There is more data coming */
#define ZULTRA_CONTINUE 0

/** This is the last of the data, finalize compression */
#define ZULTRA_FINALIZE 1

/** Default max block size */
#define ZULTRA_DEFAULT_MAX_BLOCK_SIZE 1048576

/** Streaming compression context */
typedef struct _zultra_stream_s {
   const unsigned char *next_in;       /**< pointer to next input byte */
   size_t avail_in;                    /**< size of input starting at next_in, in bytes */
   unsigned long long total_in;        /**< total input (original) bytes processed so far */

   unsigned char *next_out;            /**< pointer to output buffer */
   size_t avail_out;                   /**< size of available output buffer, in bytes */
   unsigned long long total_out;       /**< total output (compressed) bytes processed so far */

   void* (*zalloc)(void *opaque, unsigned int items, unsigned int size);   /* memory allocator */
   void (*zfree)(void *opaque, void *address);                             /* memory deallocator */
   void *opaque;                                                           /* object passed to memory functions */

   zultra_compressor_t *state;         /**< internal compressor state */
   zultra_frame_checksum_t adler;      /**< input data checksum (adler-32 for zlib, crc-32 for gzip) */
} zultra_stream_t;

/**
 * Initialize streaming compression context
 *
 * @param pStream streaming compression context
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 * @param nMaxBlockSize maximum block size (0 for default)
 *
 * @return ZULTRA_OK for success, or an error value from zultra_status_t
 */
zultra_status_t zultra_stream_init(zultra_stream_t *pStream, const unsigned int nFlags, unsigned int nMaxBlockSize);

/**
 * Set dictionary to use for compression
 *
 * @param pStream streaming compression context
 * @param pDictionaryData dictionary contents
 * @param nDictionaryDataSize size of dictionary contents, in bytes
 *
 * @return ZULTRA_OK for success, or an error value from zultra_status_t
 */
zultra_status_t zultra_stream_set_dictionary(zultra_stream_t *pStream, const void *pDictionaryData, const int nDictionaryDataSize);

/**
 * Compress data
 *
 * @param pStream streaming compression context
 * @param nDoFinalize ZULTRA_FINALIZE if the input data is the last to be compressed, ZULTRA_CONTINUE if more data is coming
 *
 * @return ZULTRA_OK for success, or an error value from zultra_status_t
 */
zultra_status_t zultra_stream_compress(zultra_stream_t *pStream, const int nDoFinalize);

/**
 * Destroy streaming compression context
 *
 * @param pStream streaming compression context
 */
void zultra_stream_end(zultra_stream_t *pStream);

/**
 * Get maximum compressed size of input(source) data
 *
 * @param nInputSize input(source) size in bytes
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 * @param nMaxBlockSize maximum block size (0 for default)
 *
 * @return maximum compressed size
 */
size_t zultra_memory_bound(size_t nInputSize, const unsigned int nFlags, unsigned int nMaxBlockSize);

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
size_t zultra_memory_compress(const unsigned char *pInputData, size_t nInputSize, unsigned char *pOutBuffer, size_t nMaxOutBufferSize, const unsigned int nFlags, unsigned int nMaxBlockSize);

#ifdef __cplusplus
}
#endif

#endif /* _STATUS_H */
