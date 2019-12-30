/*
 * private.h - definitions not visible to the application
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

#ifndef _PRIVATE_H
#define _PRIVATE_H

#include "libdivsufsort/include/divsufsort.h"
#include "huffman/huffencoder.h"
#include "huffman/bitwriter.h"

#define LCP_BITS 9
#define LCP_MAX ((1LL<<LCP_BITS) - 1)
#define LCP_SHIFT (31-LCP_BITS)
#define LCP_MASK (LCP_MAX << LCP_SHIFT)
#define POS_MASK ((1LL<<LCP_SHIFT) - 1)
#define VISITED_FLAG 0x80000000
#define EXCL_VISITED_MASK  0x7fffffff

#define NMATCHES_PER_OFFSET 8
#define MATCHES_PER_OFFSET_SHIFT 3

#define LEAVE_ALONE_MATCH_SIZE 40

#define LAST_LITERALS 1

#define MAX_SPLITS 64

/** One match */
typedef struct _zultra_match_s {
   unsigned short length;
   unsigned short offset;
} zultra_match_t;

/** Compression context */
typedef struct _zultra_compressor_s {
   /* Stream state */

   int flags;
   unsigned int max_block_size;
   const void *dictionary_data;
   int dictionary_size;

   unsigned char *in_data;
   size_t cur_in_bytes;
   int previous_block_size;

   unsigned char *out_buffer;
   size_t cur_out_index;
   size_t pending_out_bytes;
   zultra_bitwriter_t bitwriter;

   unsigned int compression_state;

   size_t cur_frame_index;
   size_t pending_frame_bytes;
   unsigned char frame_buffer[16];

   /* Block state */

   divsufsort_ctx_t divsufsort_context;
   unsigned int *intervals;
   unsigned int *pos_data;
   unsigned int *open_intervals;
   zultra_match_t *match;
   zultra_match_t *best_match;

   zultra_huffman_encoder_t literalsEncoder;
   zultra_huffman_encoder_t offsetEncoder;
} zultra_compressor_t;

#endif /* _PRIVATE_H */
