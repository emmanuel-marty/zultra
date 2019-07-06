/*
 * frame.h - gzip/zlib/deflate frame definitions
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

#ifndef _FRAME_H
#define _FRAME_H

#include <stdlib.h>

#define ZULTRA_HEADER_SIZE        4
#define ZULTRA_FRAME_SIZE         4
#define ZULTRA_FOOTER_SIZE        8

#define ZULTRA_ENCODE_ERR         (-1)

#define ZULTRA_DECODE_OK          0
#define ZULTRA_DECODE_ERR_FORMAT  (-1)
#define ZULTRA_DECODE_ERR_SUM     (-2)

#define ZULTRA_BLOCKTYPE_COMPRESSED    0
#define ZULTRA_BLOCKTYPE_UNCOMPRESSED  1
#define ZULTRA_BLOCKTYPE_LAST          2

#define BLOCK_MAXBITS_FROM_CODE(__code)   (10 + (__code))
#define ZULTRA_FRAME_CHECK(__ptr)((((__ptr)[0] ^ ((__ptr)[1] << 2)) ^ ((__ptr)[2] << 4)) & 0xff)

typedef unsigned int zultra_frame_checksum_t;

/**
 * Get compressed stream header size
 *
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 *
 * @return header size in bytes
 */
int zultra_frame_get_header_size(const unsigned int nFlags, const void *ppDictionaryData, const int nDictionarySize);

/**
 * Encode compressed stream header
 *
 * @param pFrameData encoding buffer
 * @param nMaxFrameDataSize max encoding buffer size, in bytes
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 *
 * @return number of encoded bytes, or -1 for failure
 */
int zultra_frame_encode_header(unsigned char *pFrameData, const int nMaxFrameDataSize, const unsigned int nFlags, const void *pDictionaryData, const int nDictionarySize);

/**
 * Initialize checksum
 *
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 *
 * @return initial checksum
 */
zultra_frame_checksum_t zultra_frame_init_checksum(const unsigned int nFlags);

/**
 * Update checksum
 *
 * @param nChecksum current checksum
 * @param pData data to update the checksum with
 * @param nDataSize size of the data, in bytes
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 *
 * @return updated checksum
 */
zultra_frame_checksum_t zultra_frame_update_checksum(zultra_frame_checksum_t nChecksum, const void *pData, size_t nDataSize, const unsigned int nFlags);

/**
 * Get compressed stream footer size
 *
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 *
 * @return footer size in bytes
 */
int zultra_frame_get_footer_size(const unsigned int nFlags);

/**
 * Encode compressed stream footer
 *
 * @param pFrameData encoding buffer
 * @param nMaxFrameDataSize max encoding buffer size, in bytes
 * @param nChecksum data checksum
 * @param nOriginalSize original (uncompressed) size
 * @param nFlags compression flags (ZULTRA_FLAG_xxx)
 *
 * @return number of encoded bytes, or -1 for failure
 */
int zultra_frame_encode_footer(unsigned char *pFrameData, const int nMaxFrameDataSize, const zultra_frame_checksum_t nChecksum, long long nOriginalSize, const unsigned int nFlags);

#endif /* _FRAME_H */
