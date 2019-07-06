/*
 * blockdeflate.c - optimal DEFLATE block compressor implementation
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

#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "blockdeflate.h"
#include "format.h"
#include "private.h"
#include "huffutils.h"

/* Tables for mapping every possible match offset to a huffman symbol, base value for the symbol, and number of extra bits required to encode the displacement.
 * In order to avoid a large table, values for offsets from 257 to 32768 are encoded in steps of 7 bits as the number of extra bits required starts at 7 for offset 257 */

static const unsigned short g_nOffsetSymbol[256 + 256] = {
   0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, /*256*/
   16, 17, 18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 0, 0, /*256*/
};

static const unsigned char g_nOffsetExtraBits[256 + 256] = {
   0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, /*256*/
   7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 0, 0, /*256*/
};

static const unsigned short g_nOffsetBase[256 + 256] = {
   1, 2, 3, 4, 5, 5, 7, 7, 9, 9, 9, 9, 13, 13, 13, 13, 17, 17, 17, 17, 17, 17, 17, 17, 25, 25, 25, 25, 25, 25, 25, 25, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, /*256*/
   257, 385, 513, 513, 769, 769, 1025, 1025, 1025, 1025, 1537, 1537, 1537, 1537, 2049, 2049, 2049, 2049, 2049, 2049, 2049, 2049, 3073, 3073, 3073, 3073, 3073, 3073, 3073, 3073, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 4097, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 6145, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 8193, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 12289, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 16385, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 24577, 0, 0, /*256*/
};

/* Tables for mapping every possible match length to a huffman symbol, base value for the symbol, and number of extra bits required to encode the displacement */

static const unsigned short g_nMatchLenSymbol[256] = {
   257, 258, 259, 260, 261, 262, 263, 264, 265, 265, 266, 266, 267, 267, 268, 268, 269, 269, 269, 269, 270, 270, 270, 270, 271, 271, 271, 271, 272, 272, 272, 272, 273, 273, 273, 273, 273, 273, 273, 273, 274, 274, 274, 274, 274, 274, 274, 274, 275, 275, 275, 275, 275, 275, 275, 275, 276, 276, 276, 276, 276, 276, 276, 276, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 285,
};

static const unsigned char g_nMatchLenExtraBits[256] = {
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0,
};

static const unsigned short g_nMatchLenBase[256] = {
   0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 10, 10, 12, 12, 14, 14, 16, 16, 16, 16, 20, 20, 20, 20, 24, 24, 24, 24, 28, 28, 28, 28, 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 40, 40, 40, 48, 48, 48, 48, 48, 48, 48, 48, 56, 56, 56, 56, 56, 56, 56, 56, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 255,
};

/* Reverse mapping from match length symbols to the number of displacement bits needed for each */
static const unsigned char g_nRevMatchSymbolBits[NMATCHLENSYMS] = {
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
   1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
   4, 4, 4, 4, 5, 5, 5, 5, 0,
};

/* Reverse mapping from offset symbols to the number of displacement bits needed for each */
static const unsigned char g_nRevOffsetSymbolBits[NOFFSETSYMS] = {
   0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
   9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0
};

/**
 * Get the number of bits required to represent a literal byte
 *
 * @param pCompressor compression context
 * @param nLiteralByte literal byte
 *
 * @return number of extra bits required
 */
static inline int zultra_get_literal_size(zultra_compressor_t *pCompressor, const unsigned int nLiteralByte) {
   if (nLiteralByte < 256)
      return pCompressor->literalsEncoder.nCodeLength[nLiteralByte];
   else
      return 8;
}

/**
 * Write literal byte to output (compressed) buffer
 *
 * @param pCompressor compression context
 * @param pBitWriter bit writer
 * @param nLiteralByte literal byte
 *
 * @return 0 for success, -1 for failure
 */
static int zultra_write_literal(zultra_compressor_t *pCompressor, zultra_bitwriter_t *pBitWriter, unsigned int nLiteralByte) {
   if (nLiteralByte >= 256)
      return -1;
   if (zultra_huffman_encoder_write_codeword(&pCompressor->literalsEncoder, nLiteralByte, pBitWriter) < 0)
      return -1;
   return 0;
}

/**
 * Get the number of bits required to represent a match offset
 *
 * @param pCompressor compression context
 * @param nMatchOffset match offset
 *
 * @return number of extra bits required
 */
static inline int zultra_get_offset_size(zultra_compressor_t *pCompressor, unsigned int nMatchOffset) {
   nMatchOffset--;
   if (nMatchOffset < 256)
      return pCompressor->offsetEncoder.nCodeLength[g_nOffsetSymbol[nMatchOffset]] + g_nOffsetExtraBits[nMatchOffset];
   if (nMatchOffset < 32768) {
      nMatchOffset = 256 + ((nMatchOffset - 256) >> 7);
      return pCompressor->offsetEncoder.nCodeLength[g_nOffsetSymbol[nMatchOffset]] + g_nOffsetExtraBits[nMatchOffset];
   }
   return NOFFSETSYMS;
}

/**
 * Get the huffman symbol used to represent a match offset
 *
 * @param pCompressor compression context
 * @param nMatchOffset match offset
 *
 * @return symbol
 */
static inline int zultra_get_offset_symbol(zultra_compressor_t *pCompressor, unsigned int nMatchOffset) {
   nMatchOffset--;
   if (nMatchOffset < 256)
      return g_nOffsetSymbol[nMatchOffset];
   if (nMatchOffset < 32768)
      return g_nOffsetSymbol[256 + ((nMatchOffset - 256) >> 7)];
   return NOFFSETSYMS;
}

/**
 * Write match offset to output (compressed) buffer
 *
 * @param pCompressor compression context
 * @param pBitWriter bit writer
 * @param nMatchOffset match offset
 *
 * @return 0 for success, -1 for failure
 */
static int zultra_write_offset(zultra_compressor_t *pCompressor, zultra_bitwriter_t *pBitWriter, const unsigned int nMatchOffset) {
   unsigned int nSymbol;
   unsigned int nBase;
   unsigned int nExtraBits;
   unsigned int nDisp;

   unsigned int nMatchOffsetIdx = nMatchOffset - 1;
   if (nMatchOffsetIdx < 256) {
      nSymbol = g_nOffsetSymbol[nMatchOffsetIdx];
      nBase = g_nOffsetBase[nMatchOffsetIdx];
      nExtraBits = g_nOffsetExtraBits[nMatchOffsetIdx];
   }
   else if (nMatchOffsetIdx < 32768) {
      nMatchOffsetIdx = 256 + ((nMatchOffsetIdx - 256) >> 7);
      nBase = g_nOffsetBase[nMatchOffsetIdx];
      nSymbol = g_nOffsetSymbol[nMatchOffsetIdx];
      nExtraBits = g_nOffsetExtraBits[nMatchOffsetIdx];
   }
   else {
      return -1;
   }

   nDisp = nMatchOffset - nBase;

   if (zultra_huffman_encoder_write_codeword(&pCompressor->offsetEncoder, nSymbol, pBitWriter) < 0)
      return -1;
   if (zultra_bitwriter_put_bits(pBitWriter, nDisp, nExtraBits) < 0)
      return -1;
   return 0;
}

/**
 * Get the huffman symbol used to represent a match length
 *
 * @param pCompressor compression context
 * @param nLength encoded match length (length - MIN_MATCH_SIZE)
 *
 * @return symbol
 */
static inline int zultra_get_varlen_symbol(zultra_compressor_t *pCompressor, unsigned int nLength) {
   if (nLength > 255) nLength = 255;
   return g_nMatchLenSymbol[nLength];
}

/**
 * Get the number of bits required to represent a match length
 *
 * @param pCompressor compression context
 * @param nLength encoded match length (length - MIN_MATCH_SIZE)
 *
 * @return number of bits required
 */
static inline int zultra_get_varlen_size(zultra_compressor_t *pCompressor, unsigned int nLength) {
   if (nLength > 255) nLength = 255;
   return pCompressor->literalsEncoder.nCodeLength[g_nMatchLenSymbol[nLength]] + g_nMatchLenExtraBits[nLength];
}

/**
 * Write match length to output (compressed) buffer
 *
 * @param pCompressor compression context
 * @param pBitWriter bit writer
 * @param nLength encoded match length (length - MIN_MATCH_SIZE)
 *
 * @return 0 for success, -1 for failure
 */
static int zultra_write_varlen(zultra_compressor_t *pCompressor, zultra_bitwriter_t *pBitWriter, const unsigned int nLength) {
   unsigned int nLengthIdx = nLength;
   if (nLengthIdx > 255) nLengthIdx = 255;

   unsigned int nSymbol = g_nMatchLenSymbol[nLengthIdx];
   unsigned int nBase = g_nMatchLenBase[nLengthIdx];
   unsigned int nExtraBits = g_nMatchLenExtraBits[nLengthIdx];
   unsigned int nDisp = nLength - nBase;

   if (zultra_huffman_encoder_write_codeword(&pCompressor->literalsEncoder, nSymbol, pBitWriter) < 0)
      return -1;
   if (zultra_bitwriter_put_bits(pBitWriter, nDisp, nExtraBits) < 0)
      return -1;
   return 0;
}

/**
 * Attempt to pick optimal matches, so as to produce the smallest possible output that decompresses to the same input
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 */
static void zultra_optimize_matches_lwd(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nEndOffset) {
   int *cost = (int*)pCompressor->pos_data;  /* Reuse */
   zultra_match_t *best_match = pCompressor->best_match;
   int nLastLiteralsOffset;
   int i;
   int nCachedVarlenSize[LEAVE_ALONE_MATCH_SIZE];

   if (nEndOffset <= nStartOffset) return;

   for (i = 0; i < LEAVE_ALONE_MATCH_SIZE; i++)
      nCachedVarlenSize[i] = zultra_get_varlen_size(pCompressor, i);

   cost[nEndOffset - 1] = zultra_get_literal_size(pCompressor, pInWindow[nEndOffset - 1]);
   best_match[nEndOffset - 1].length = 0;
   best_match[nEndOffset - 1].offset = 0;
   nLastLiteralsOffset = nEndOffset;

   for (i = nEndOffset - 2; i != (nStartOffset - 1); i--) {
      int nBestCost, nBestMatchLen, nBestMatchOffset;

      nBestCost = zultra_get_literal_size(pCompressor, pInWindow[i]) + cost[i + 1];
      nBestMatchLen = 0;
      nBestMatchOffset = 0;

      const zultra_match_t *pMatch = pCompressor->match + (i << MATCHES_PER_OFFSET_SHIFT);
      int m;

      for (m = 0; m < NMATCHES_PER_OFFSET && pMatch[m].length >= MIN_MATCH_SIZE; m++) {
         int nOffsetSize = zultra_get_offset_size(pCompressor, pMatch[m].offset);
         int nMatchLen = pMatch[m].length;

         if ((i + nMatchLen) > (nEndOffset - LAST_LITERALS))
            nMatchLen = nEndOffset - LAST_LITERALS - i;

         if (pMatch[m].length >= LEAVE_ALONE_MATCH_SIZE) {
            int nCurCost;

            nCurCost = zultra_get_varlen_size(pCompressor, nMatchLen - MIN_MATCH_SIZE);
            nCurCost += nOffsetSize + cost[i + nMatchLen];

            if (nBestCost > nCurCost) {
               nBestCost = nCurCost;
               nBestMatchLen = nMatchLen;
               nBestMatchOffset = pMatch[m].offset;
            }
         }
         else {
            int k;

            for (k = nMatchLen; k >= MIN_MATCH_SIZE; k--) {
               int nCurCost;

               nCurCost = nCachedVarlenSize[k - MIN_MATCH_SIZE];
               nCurCost += nOffsetSize + cost[i + k];

               if (nBestCost > nCurCost) {
                  nBestCost = nCurCost;
                  nBestMatchLen = k;
                  nBestMatchOffset = pMatch[m].offset;
               }
            }
         }
      }

      if (nBestMatchLen >= MIN_MATCH_SIZE)
         nLastLiteralsOffset = i;

      cost[i] = nBestCost;
      best_match[i].length = nBestMatchLen;
      best_match[i].offset = nBestMatchOffset;
   }
}

/**
 * Update entropy of offsets and lengths, to build tentative huffman tables and have initial code lengths for the optimizer
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 */
static void zultra_build_initial_entropy_lwd(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nEndOffset) {
   int i;

   for (i = nStartOffset; i < nEndOffset; ) {
      const zultra_match_t *pMatch = pCompressor->match + (i << MATCHES_PER_OFFSET_SHIFT);
      if (pMatch->length >= MIN_MATCH_SIZE) {
         unsigned int nMatchOffset = pMatch[0].offset;
         unsigned int nMatchLen = pMatch[0].length;
         unsigned int nEncodedMatchLen = nMatchLen - MIN_MATCH_SIZE;

         /* Count match length */
         pCompressor->literalsEncoder.nEntropy[zultra_get_varlen_symbol(pCompressor, nEncodedMatchLen)]++;

         /* Count match offset */
         pCompressor->offsetEncoder.nEntropy[zultra_get_offset_symbol(pCompressor, nMatchOffset)]++;
         i += nMatchLen;
      }
      else {
         /* Count literal value */
         unsigned int nByte = pInWindow[i];
         if (nByte < 256)
            pCompressor->literalsEncoder.nEntropy[nByte]++;
         i++;
      }
   }

   /* Count EOD marker */
   pCompressor->literalsEncoder.nEntropy[NEODMARKERSYM]++;
}

/**
 * Update entropy of offsets and lengths, after running the optimizer and before creating the huffman tables
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 */
static void zultra_build_final_entropy_lwd(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nEndOffset) {
   int i;

   for (i = nStartOffset; i < nEndOffset; ) {
      const zultra_match_t *pMatch = pCompressor->best_match + i;

      if (pMatch->length >= MIN_MATCH_SIZE) {
         unsigned int nMatchOffset = pMatch->offset;
         unsigned int nMatchLen = pMatch->length;
         unsigned int nEncodedMatchLen = nMatchLen - MIN_MATCH_SIZE;

         /* Count match length */
         pCompressor->literalsEncoder.nEntropy[zultra_get_varlen_symbol(pCompressor, nEncodedMatchLen)]++;

         /* Count match offset */
         pCompressor->offsetEncoder.nEntropy[zultra_get_offset_symbol(pCompressor, nMatchOffset)]++;
         i += nMatchLen;
      }
      else {
         /* Count literal value */
         unsigned int nByte = pInWindow[i];
         if (nByte < 256)
            pCompressor->literalsEncoder.nEntropy[nByte]++;
         i++;
      }
   }

   /* Count EOD marker */
   pCompressor->literalsEncoder.nEntropy[NEODMARKERSYM]++;
}

/**
 * Apply optimizations once the final code lengths are known
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 */
static void zultra_post_optimize_block_lwd(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nEndOffset) {
   int i;

   for (i = nStartOffset; i < nEndOffset; ) {
      zultra_match_t *pMatch = pCompressor->best_match + i;

      if (pMatch->length >= MIN_MATCH_SIZE) {
         unsigned int nMatchOffset = pMatch->offset;
         unsigned int nMatchLen = pMatch->length;
         unsigned int nEncodedMatchLen = nMatchLen - MIN_MATCH_SIZE;
         unsigned int nMatchCost, nLiteralsCost;
         unsigned int j;
         int nStartIdx = i;

         i += nMatchLen;
         if (nMatchOffset < MIN_OFFSET || nMatchOffset > MAX_OFFSET)
            continue;

         /* Add up the cost of encoding the match */
         nMatchCost = zultra_get_varlen_size(pCompressor, nEncodedMatchLen);
         nMatchCost += zultra_get_offset_size(pCompressor, nMatchOffset);

         /* Add up the cost of encoding match as literals */
         nLiteralsCost = 0;
         for (j = 0; j < nMatchLen && nLiteralsCost < nMatchCost; j++) {
            unsigned int nCurLiteralCost = pCompressor->literalsEncoder.nCodeLength[pInWindow[nStartIdx + j]];
            if (nCurLiteralCost == 0) {
               /* We didn't select this symbol to be in the huffman table, keep match encoding. */
               nLiteralsCost = -1;
               break;
            }

            nLiteralsCost += nCurLiteralCost;
         }

         if (nLiteralsCost == -1) continue;

         if (nLiteralsCost < nMatchCost) {
            /* Cheaper to encode the match as literals, rewrite it */
            for (j = 0; j < nMatchLen; j++) {
               pCompressor->best_match[nStartIdx + j].length = 0;
            }
         }
      }
      else {
         i++;
      }
   }
}

/**
 * Emit block of compressed data
 *
 * @param pCompressor compression context
 * @param pBitWriter bit writer
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 *
 * @return end of compressed data in output buffer, or -1 if the data is uncompressible
 */
static int zultra_write_block_lwd(zultra_compressor_t *pCompressor, zultra_bitwriter_t *pBitWriter, const unsigned char *pInWindow, const int nStartOffset, const int nEndOffset) {
   int i;

   for (i = nStartOffset; i < nEndOffset; ) {
      const zultra_match_t *pMatch = pCompressor->best_match + i;

      if (pMatch->length >= MIN_MATCH_SIZE) {
         unsigned int nMatchOffset = pMatch->offset;
         unsigned int nMatchLen = pMatch->length;
         unsigned int nEncodedMatchLen = nMatchLen - MIN_MATCH_SIZE;

         if (nMatchOffset < MIN_OFFSET || nMatchOffset > MAX_OFFSET)
            return -1;

         /* Match: emit match length */
         if (zultra_write_varlen(pCompressor, pBitWriter, nEncodedMatchLen) < 0)
            return -1;

         /* Emit match offset */
         if (zultra_write_offset(pCompressor, pBitWriter, nMatchOffset) < 0)
            return -1;
         i += nMatchLen;
      }
      else {
         /* Literal: emit literal value */
         if (zultra_write_literal(pCompressor, pBitWriter, pInWindow[i]) < 0)
            return -1;
         i++;
      }
   }

   /* Emit EOD marker */
   if (zultra_huffman_encoder_write_codeword(&pCompressor->literalsEncoder, NEODMARKERSYM, pBitWriter) < 0)
      return -1;

   return 0;
}

/**
 * Prepare parsed block of data to calculate the estimated cost of encoding it
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset starting offset in window, of input bytes to evaluate (including number of previously compressed bytes)
 * @param nInDataSize number of input bytes to evaluate
 *
 * @return 0 for success, -1 in case of an error
 */
int zultra_block_prepare_cost_evaluation(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nInDataSize) {
   if (zultra_huffman_encoder_init(&pCompressor->literalsEncoder, NLITERALSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0 ||
      zultra_huffman_encoder_init(&pCompressor->offsetEncoder, NOFFSETSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0)
      return -1;

   /* Account for entropy using greedy parsing */
   zultra_build_initial_entropy_lwd(pCompressor, pInWindow, nStartOffset, nStartOffset + nInDataSize);
   return 0;
}

/**
 * Estimate compressed size of one parsed block of data using static huffman tables
 *
 * @param pLiteralsEncoder literals encoding context
 * @param pOffsetEncoder offset encoding context
 * @param pStaticCost where to store the compressed cost in bits when using a static huffman table
 *
 * @return 0 for success, -1 in case of an error
 */
int zultra_block_evaluate_static_cost(zultra_huffman_encoder_t *pLiteralsEncoder, zultra_huffman_encoder_t *pOffsetEncoder, int *pStaticCost) {
   int i, nStaticCost = 0;
   int nStaticLiteralCodeLength[NLITERALSYMS];

   /* Build static huffman code lengths (RFC 1951 section 3.2.6) */
   for (i = 0; i < 144; i++)
      nStaticLiteralCodeLength[i] = 8;
   for (; i < 256; i++)
      nStaticLiteralCodeLength[i] = 9;
   for (; i < 280; i++)
      nStaticLiteralCodeLength[i] = 7;
   for (; i < NLITERALSYMS; i++)
      nStaticLiteralCodeLength[i] = 8;

   for (i = 0; i < NMATCHLENSYMSTART; i++) {
      nStaticCost += pLiteralsEncoder->nEntropy[i] * nStaticLiteralCodeLength[i];
   }
   for (; i < (NMATCHLENSYMSTART + NMATCHLENSYMS); i++) {
      nStaticCost += pLiteralsEncoder->nEntropy[i] * (nStaticLiteralCodeLength[i] + g_nRevMatchSymbolBits[i - NMATCHLENSYMSTART]);
   }

   for (i = 0; i < NOFFSETSYMS; i++) {
      nStaticCost += pOffsetEncoder->nEntropy[i] * (5 /* fixed offset codeword size */ + g_nRevOffsetSymbolBits[i]);
   }

   /* Report costs */
   *pStaticCost = nStaticCost + 3;
   return 0;
}

/**
 * Estimate compressed size of one parsed block of data using dynamic huffman tables
 *
 * @param pLiteralsEncoder literals encoding context
 * @param pOffsetEncoder offset encoding context
 * @param pDynamicCost where to store the compressed cost in bits when using a dynamic huffman table
 *
 * @return 0 for success, -1 in case of an error
 */
int zultra_block_evaluate_dynamic_cost(zultra_huffman_encoder_t *pLiteralsEncoder, zultra_huffman_encoder_t *pOffsetEncoder, int *pDynamicCost) {
   zultra_huffman_encoder_t tablesEncoder;
   int i, nDynamicCost = 0;
   int nCodeLength[NLITERALSYMS + NOFFSETSYMS];
   int nLiteralSyms, nOffsetSyms, nCodeLenSyms;

   for (i = 0; i < NMATCHLENSYMSTART; i++) {
      nDynamicCost += pLiteralsEncoder->nEntropy[i] * pLiteralsEncoder->nCodeLength[i];
   }
   for (; i < (NMATCHLENSYMSTART + NMATCHLENSYMS); i++) {
      nDynamicCost += pLiteralsEncoder->nEntropy[i] * (pLiteralsEncoder->nCodeLength[i] + g_nRevMatchSymbolBits[i - NMATCHLENSYMSTART]);
   }

   for (i = 0; i < NOFFSETSYMS; i++) {
      nDynamicCost += pOffsetEncoder->nEntropy[i] * (pOffsetEncoder->nCodeLength[i] + g_nRevOffsetSymbolBits[i]);
   }

   /* Build the code lengths table */
   nLiteralSyms = zultra_huffman_encoder_get_defined_var_lengths_count(pLiteralsEncoder, 257);
   nOffsetSyms = zultra_huffman_encoder_get_defined_var_lengths_count(pOffsetEncoder, 1);

   memcpy(nCodeLength, pLiteralsEncoder->nCodeLength, nLiteralSyms * sizeof(int));
   memcpy(nCodeLength + nLiteralSyms, pOffsetEncoder->nCodeLength, nOffsetSyms * sizeof(int));

   zultra_huffman_encoder_init(&tablesEncoder, NCODELENSYMS /* symbols */, 7 /* max code length */, 0 /* default code length */);
   zultra_huffman_encoder_update_var_lengths_entropy(&tablesEncoder, nLiteralSyms + nOffsetSyms, nCodeLength, 7);
   zultra_huffman_encoder_estimate_dynamic_codelens(&tablesEncoder);

   /* Add the cost of writing the symbol counts */
   nDynamicCost += 5 + 5 + 4;

   /* Add up the cost of the code lengths table symbols */
   nCodeLenSyms = zultra_huffman_encoder_get_raw_table_size(&tablesEncoder);
   nDynamicCost += NCODELENBITS * nCodeLenSyms;

   /* Finaly, add up the cost of the code lengths table itself */
   nDynamicCost += zultra_huffman_encoder_get_var_lengths_size(&tablesEncoder, nLiteralSyms + nOffsetSyms, nCodeLength, MAX_CODES_MASK);

   /* Report costs */
   *pDynamicCost = nDynamicCost + 3;
   return 0;
}

/**
 * Recursively find split points in one parsed sub-block of data, where it would be better to stop the block and start sending new entropy codes
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset starting offset in window, of input bytes to look at (including number of previously compressed bytes)
 * @param nInDataSize number of input bytes to look at
 * @param nDepth current depth
 * @param nMaxSplits maximum number of splits to create
 * @param nSplitCount pointer to current number of splits
 * @param pSplitOffset pointer to array of split positions to fill out
 *
 * @return 0 for success, or -1 if an error occured
 */
static int zultra_compressor_split_subblock_recursive(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nInDataSize, const int nDepth,
                                                      const int nMaxSplits, int *pSplitCount, int *pSplitOffset) {
   unsigned int stat[18];
   unsigned int newStat[18];
   unsigned int nNumStats = 0;
   unsigned int nNumNewStats = 0;
   int nTotalDynamicCost = 0;
   int i, j, nLastGoodSplitIdx = -1;

   if ((*pSplitCount) >= nMaxSplits)
      return 0;

   if (nDepth >= 6 || nInDataSize < 8192)
      return 0;

   /**
    * The block splitting heuristics are inspired by a model by Eric Bigger in libdeflate.
    *
    * Notable improvements, from highest to lowest impact on the compression ratio:
    * (1) when we find what looks like a good split point, we evaluate the 'to the left'+'to the right' vs. total encoding costs
    *     using the huffman codelengths produced by a greedy parse. we pick the most beneficial outcome.
    * (2) the split is performed after the last good point, before we detect that the measured distribution has drifted too
    *     far from the expectations. 
    * (3) we measure more data points.
    */
   memset(stat, 0, 18 * sizeof(unsigned int));
   memset(newStat, 0, 18 * sizeof(unsigned int));

   if (zultra_block_prepare_cost_evaluation(pCompressor, pInWindow, nStartOffset, nInDataSize) < 0)
      return -1;

   if (zultra_huffman_encoder_estimate_dynamic_codelens(&pCompressor->literalsEncoder) < 0 ||
      zultra_huffman_encoder_estimate_dynamic_codelens(&pCompressor->offsetEncoder) < 0 ||
      zultra_block_evaluate_dynamic_cost(&pCompressor->literalsEncoder, &pCompressor->offsetEncoder, &nTotalDynamicCost) < 0)
      return -1;

   zultra_huffman_encoder_t totalLiteralsEncoder = pCompressor->literalsEncoder;
   zultra_huffman_encoder_t totalOffsetEncoder = pCompressor->offsetEncoder;
   zultra_huffman_encoder_t leftLiteralsEncoder, leftOffsetEncoder;
   zultra_huffman_encoder_t rightLiteralsEncoder, rightOffsetEncoder;
   int nLastLeftEndOffset = nStartOffset;
   int nBestSplitOffset = nStartOffset + nInDataSize;
   int nBestSplitDelta = 0;

   if (zultra_huffman_encoder_init(&leftLiteralsEncoder, NLITERALSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0 ||
      zultra_huffman_encoder_init(&leftOffsetEncoder, NOFFSETSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0 ||
      zultra_huffman_encoder_init(&rightLiteralsEncoder, NLITERALSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0 ||
      zultra_huffman_encoder_init(&rightOffsetEncoder, NOFFSETSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0)
      return nInDataSize;

   for (i = nStartOffset; i < nStartOffset + nInDataSize; ) {
      const zultra_match_t *pMatch = pCompressor->match + (i << MATCHES_PER_OFFSET_SHIFT);
      if (pMatch->length >= MIN_MATCH_SIZE) {
         unsigned int nMatchLen = pMatch[0].length;

         if (nMatchLen >= 9)
            newStat[17]++;
         else
            newStat[16]++;
         nNumNewStats++;

         i += nMatchLen;
      }
      else {
         unsigned int nByte = pInWindow[i];
         newStat[((nByte >> 4) & 0xc) | (nByte & 0x3)]++;

         nNumNewStats++;
         i++;
      }

      if (nNumNewStats >= 256 && (i - nStartOffset) >= 512) {
         unsigned int nTotalDelta = 0;

         if (nNumStats) {
            for (j = 0; j < 18; j++) {
               unsigned int expected = stat[j] * nNumNewStats;
               unsigned int actual = newStat[j] * nNumStats;
               unsigned int nDelta;

               if (expected > actual)
                  nDelta = expected - actual;
               else
                  nDelta = actual - expected;
               nTotalDelta += nDelta;
            }

            if ((nTotalDelta / nNumNewStats) >= (nNumStats * 45 / 100) && nLastGoodSplitIdx >= 0) {
               /* The actual distribution has drifted away enough from the expected distribution to check for a block split */

               int nLeftDynamicCost = 0, nRightDynamicCost = 0;

               /* We only add up the entropy for the section of the block between our last stop point and the current check point, instead of scanning the
                * whole thing from the start, and we add that entropy to what we already accumulated for the left part of the split. Then, we calculate the
                * entropy for the right part by substracting the total entropy from the accumulated entropy for the left part. As we only stop to check for
                * splits after a match or a literal but never in the middle of a match, the entropy totals are as accurate as if we scanned the whole block
                * all the time. */

               if (zultra_block_prepare_cost_evaluation(pCompressor, pInWindow, nLastLeftEndOffset, nLastGoodSplitIdx - nLastLeftEndOffset) >= 0) {
                  for (j = 0; j < NLITERALSYMS; j++)
                     leftLiteralsEncoder.nEntropy[j] += pCompressor->literalsEncoder.nEntropy[j];
                  for (j = 0; j < NOFFSETSYMS; j++)
                     leftOffsetEncoder.nEntropy[j] += pCompressor->offsetEncoder.nEntropy[j];
                  leftLiteralsEncoder.nEntropy[NEODMARKERSYM] = 1;   /* Don't distort the entropy of the block end marker */

                  for (j = 0; j < NLITERALSYMS; j++)
                     rightLiteralsEncoder.nEntropy[j] = totalLiteralsEncoder.nEntropy[j] - leftLiteralsEncoder.nEntropy[j];
                  for (j = 0; j < NOFFSETSYMS; j++)
                     rightOffsetEncoder.nEntropy[j] = totalOffsetEncoder.nEntropy[j] - leftOffsetEncoder.nEntropy[j];
                  rightLiteralsEncoder.nEntropy[NEODMARKERSYM] = 1;   /* Don't distort the entropy of the block end marker */

                  if (zultra_huffman_encoder_estimate_dynamic_codelens(&leftLiteralsEncoder) >= 0 &&
                     zultra_huffman_encoder_estimate_dynamic_codelens(&leftOffsetEncoder) >= 0 &&
                     zultra_block_evaluate_dynamic_cost(&leftLiteralsEncoder, &leftOffsetEncoder, &nLeftDynamicCost) >= 0 &&
                     zultra_huffman_encoder_estimate_dynamic_codelens(&rightLiteralsEncoder) >= 0 &&
                     zultra_huffman_encoder_estimate_dynamic_codelens(&rightOffsetEncoder) >= 0 &&
                     zultra_block_evaluate_dynamic_cost(&rightLiteralsEncoder, &rightOffsetEncoder, &nRightDynamicCost) >= 0) {
                     int nDelta = nTotalDynamicCost - (nLeftDynamicCost + nRightDynamicCost);
                     if (nDelta >= 0) {
                        if (nBestSplitOffset == (nStartOffset + nInDataSize) || nBestSplitDelta < nDelta) {
                           nBestSplitOffset = nLastGoodSplitIdx;
                           nBestSplitDelta = nDelta;
                        }
                     }
                  }

                  nLastLeftEndOffset = nLastGoodSplitIdx;
               }
            }
         }

         for (j = 0; j < 18; j++) {
            nNumStats += newStat[j];
            stat[j] += newStat[j];
            newStat[j] = 0;
         }
         nNumNewStats = 0;
         nLastGoodSplitIdx = i;
      }
   }

   if (nBestSplitOffset != (nStartOffset + nInDataSize)) {
      if (zultra_compressor_split_subblock_recursive(pCompressor, pInWindow, nStartOffset, nBestSplitOffset - nStartOffset, nDepth + 1, nMaxSplits, pSplitCount, pSplitOffset) < 0)
         return -1;
      if ((*pSplitCount) < nMaxSplits) {
         pSplitOffset[(*pSplitCount)++] = nBestSplitOffset;
      }
      if (zultra_compressor_split_subblock_recursive(pCompressor, pInWindow, nBestSplitOffset, (nInDataSize + nStartOffset) - nBestSplitOffset, nDepth + 1, nMaxSplits, pSplitCount, pSplitOffset) < 0)
         return -1;
   }

   return 0;
}

/**
 * Find split points in one parsed block of data, where it would be better to stop the block and start sending new entropy codes
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset starting offset in window, of input bytes to look at (including number of previously compressed bytes)
 * @param nInDataSize number of input bytes to look at
 * @param nMaxSplits maximum number of splits to create
 * @param pSplitOffset pointer to array of split positions to fill out
 *
 * @return number of splits written in array, or -1 if an error occured
 */
int zultra_block_split(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nInDataSize,
                       const int nMaxSplits, int *pSplitOffset) {
   int nNumSplits = 0;

   int nResult = zultra_compressor_split_subblock_recursive(pCompressor, pInWindow, nStartOffset, nInDataSize, 0,
      nMaxSplits - 1, &nNumSplits, pSplitOffset);
   if (!nResult && nNumSplits < nMaxSplits) {
      pSplitOffset[nNumSplits++] = nStartOffset + nInDataSize;
      return nNumSplits;
   }
   else {
      return nResult;
   }
}

/**
 * Select the most optimal matches, reduce the token count if possible, and then emit a block of compressed LZWIDE data
 *
 * @param pCompressor compression context
 * @param pBitWriter bit writer
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset starting offset in window, of input bytes to compress (including number of previously compressed bytes)
 * @param nInDataSize number of input bytes to compress
 * @param nIsDynamic 1 to generate and compress with dynamic huffman tables, 0 to use static huffman tables
 *
 * @return end of compressed data in output buffer, or -1 if the data is uncompressible
 */
int zultra_block_deflate(zultra_compressor_t *pCompressor, zultra_bitwriter_t *pBitWriter, const unsigned char *pInWindow, const int nStartOffset, const int nInDataSize, const int nIsDynamic) {
   zultra_huffman_encoder_t tablesEncoder;
   int nCodeLength[NLITERALSYMS + NOFFSETSYMS];
   int i, nLiteralSyms, nOffsetSyms;

   if (zultra_huffman_encoder_init(&pCompressor->literalsEncoder, NLITERALSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0 ||
       zultra_huffman_encoder_init(&pCompressor->offsetEncoder, NOFFSETSYMS /* symbols*/, 15 /* max code length */, 0 /* default code length */) < 0)
      return -1;

   if (!nIsDynamic) {
      /* Build static huffman tables (RFC 1951 section 3.2.6) */

      for (i = 0; i < 144; i++)
         pCompressor->literalsEncoder.nCodeLength[i] = 8;
      for (; i < 256; i++)
         pCompressor->literalsEncoder.nCodeLength[i] = 9;
      for (; i < 280; i++)
         pCompressor->literalsEncoder.nCodeLength[i] = 7;
      for (; i < NLITERALSYMS; i++)
         pCompressor->literalsEncoder.nCodeLength[i] = 8;

      for (i = 0; i < NOFFSETSYMS; i++)
         pCompressor->offsetEncoder.nCodeLength[i] = 5;

      if (zultra_huffman_encoder_build_static_codewords(&pCompressor->literalsEncoder) < 0)
         return -1;
      if (zultra_huffman_encoder_build_static_codewords(&pCompressor->offsetEncoder) < 0)
         return -1;

      /* Optimize using the actual bit lengths */
      zultra_optimize_matches_lwd(pCompressor, pInWindow, nStartOffset, nStartOffset + nInDataSize);
   }
   else {
      const int nConvergencePasses = 3;

      /* Account for entropy using initial (greedy) parsing */
      zultra_build_initial_entropy_lwd(pCompressor, pInWindow, nStartOffset, nStartOffset + nInDataSize);

      /* Build tentative huffman tables */
      if (zultra_huffman_encoder_build_dynamic_codewords(&pCompressor->literalsEncoder) < 0)
         return -1;
      if (zultra_huffman_encoder_build_dynamic_codewords(&pCompressor->offsetEncoder) < 0)
         return -1;

      for (int j = 0; j <= nConvergencePasses; j++) {
         /* Give default length to codewords that were unused, in case the optimizer decides to use them */
         for (i = 0; i < NLITERALSYMS; i++) {
            if (pCompressor->literalsEncoder.nCodeLength[i] == 0)
               pCompressor->literalsEncoder.nCodeLength[i] = 9;
         }

         for (i = 0; i < NOFFSETSYMS; i++) {
            if (pCompressor->offsetEncoder.nCodeLength[i] == 0)
               pCompressor->offsetEncoder.nCodeLength[i] = 6;
         }

         /* Re-optimize using the actual bit lengths for each codeword */
         zultra_optimize_matches_lwd(pCompressor, pInWindow, nStartOffset, nStartOffset + nInDataSize);

         /* Reset entropy and count it again with the actual codewords now used by the optimizer pass above */
         for (i = 0; i < NLITERALSYMS; i++)
            pCompressor->literalsEncoder.nEntropy[i] = 0;
         for (i = 0; i < NOFFSETSYMS; i++)
            pCompressor->offsetEncoder.nEntropy[i] = 0;
         zultra_build_final_entropy_lwd(pCompressor, pInWindow, nStartOffset, nStartOffset + nInDataSize);

         if (j == nConvergencePasses) {
            int nOffsetLens = 0;
            for (i = 0; nOffsetLens < 2 && i < NOFFSETSYMS - 2; i++) {
               if (pCompressor->offsetEncoder.nEntropy[i])
                  nOffsetLens++;
            }

            /*
             * Always emit offset codewords, even if none are needed, in order to work around an old zlib inflate bug fixed in v1.2.1.1.
             * See https://github.com/madler/zlib/commit/f0e76a6634eb26e3ddc6dfc6f2489553eff8c8f4#diff-c6c7151cbf1438b58966977f9ed524f4
             */
            if (nOffsetLens == 0) {
               pCompressor->offsetEncoder.nEntropy[0] = pCompressor->offsetEncoder.nEntropy[1] = 1;
            }
            else if (nOffsetLens == 1) {
               if (pCompressor->offsetEncoder.nEntropy[0])
                  pCompressor->offsetEncoder.nEntropy[1] = 1;
               else
                  pCompressor->offsetEncoder.nEntropy[0] = 1;
            }
         }

         /* Build final huffman tables for literals and offsets */
         if (zultra_huffman_encoder_build_dynamic_codewords(&pCompressor->literalsEncoder) < 0)
            return -1;
         if (zultra_huffman_encoder_build_dynamic_codewords(&pCompressor->offsetEncoder) < 0)
            return -1;
      }

      /* Apply optimizations now that we know the final code lengths */
      zultra_post_optimize_block_lwd(pCompressor, pInWindow, nStartOffset, nStartOffset + nInDataSize);

      /* Attempt to optimize the final huffman tables so that the lengths compress better with RLE */
      zultra_huffman_encoder_t optLiteralsEncoder = pCompressor->literalsEncoder;
      zultra_huffman_encoder_t optOffsetEncoder = pCompressor->offsetEncoder;

      int nCurTotalBitCost = 0;
      zultra_block_evaluate_dynamic_cost(&optLiteralsEncoder, &optOffsetEncoder, &nCurTotalBitCost);
      zultra_huffman_encoder_optimize_for_rle(NLITERALSYMS, optLiteralsEncoder.nEntropy, nCodeLength /* Temporary storage space, must be >= NLITERALSYMS */);
      zultra_huffman_encoder_optimize_for_rle(NOFFSETSYMS, optOffsetEncoder.nEntropy, nCodeLength /* Temporary storage space, must be >= NOFFSETSYMS */);
      if (zultra_huffman_encoder_build_dynamic_codewords(&optLiteralsEncoder) < 0)
         return -1;
      if (zultra_huffman_encoder_build_dynamic_codewords(&optOffsetEncoder) < 0)
         return -1;

      int nOptTotalBitCost = 0;
      zultra_block_evaluate_dynamic_cost(&optLiteralsEncoder, &optOffsetEncoder, &nOptTotalBitCost);
      /* Smaller tables + data with the optimization? */
      if (nOptTotalBitCost < nCurTotalBitCost) {
         /* If so, replace the final tables with the optimized ones */
         pCompressor->literalsEncoder = optLiteralsEncoder;
         pCompressor->offsetEncoder = optOffsetEncoder;
      }

      /* Get the number of actually used literals and offset symbols */
      nLiteralSyms = zultra_huffman_encoder_get_defined_var_lengths_count(&pCompressor->literalsEncoder, 257 /* min. encoded value */);
      nOffsetSyms = zultra_huffman_encoder_get_defined_var_lengths_count(&pCompressor->offsetEncoder, 1 /* min. encoded value */);

      memcpy(nCodeLength, pCompressor->literalsEncoder.nCodeLength, nLiteralSyms * sizeof(int));
      memcpy(nCodeLength + nLiteralSyms, pCompressor->offsetEncoder.nCodeLength, nOffsetSyms * sizeof(int));

      /* Build huffman table for encoding the literals and offsets tables */
      zultra_huffman_encoder_init(&tablesEncoder, NCODELENSYMS /* symbols */, 7 /* max code length */, 0 /* default code length */);

      /* Find combination of run-length encoding codes that yields the smallest tables for this block */
      int nBestTablesCost = 0, nBestCodesMask = -1;
      for (int nCurCodesMask = 0; nCurCodesMask <= MAX_CODES_MASK; (nCurCodesMask >= 7) ? (nCurCodesMask += 2) : (nCurCodesMask++)) {
         int nCurTablesCost;

         zultra_huffman_encoder_update_var_lengths_entropy(&tablesEncoder, nLiteralSyms + nOffsetSyms, nCodeLength, nCurCodesMask);
         zultra_huffman_encoder_build_dynamic_codewords(&tablesEncoder);
         
         nCurTablesCost = zultra_huffman_encoder_get_var_lengths_size(&tablesEncoder, nLiteralSyms + nOffsetSyms, nCodeLength, nCurCodesMask);
         if (nBestCodesMask == -1 || nBestTablesCost >= nCurTablesCost) {
            nBestCodesMask = nCurCodesMask;
            nBestTablesCost = nCurTablesCost;
         }

         /* Reset entropy for next loop, or for finalizing the table below */
         for (i = 0; i < NCODELENSYMS; i++)
            tablesEncoder.nEntropy[i] = 0;
      }

      zultra_huffman_encoder_update_var_lengths_entropy(&tablesEncoder, nLiteralSyms + nOffsetSyms, nCodeLength, nBestCodesMask);
      zultra_huffman_encoder_build_dynamic_codewords(&tablesEncoder);

      /* Write all huffman code length tables */
      int nCodeLenSyms = zultra_huffman_encoder_get_raw_table_size(&tablesEncoder);
      if (nLiteralSyms > NVALIDLITERALSYMS ||
         nOffsetSyms > NVALIDOFFSETSYMS ||
         nCodeLenSyms > NCODELENSYMS ||
         zultra_bitwriter_put_bits(pBitWriter, nLiteralSyms - 257, 5) < 0 ||
         zultra_bitwriter_put_bits(pBitWriter, nOffsetSyms - 1, 5) < 0 ||
         zultra_bitwriter_put_bits(pBitWriter, nCodeLenSyms - 4, 4) < 0)
         return -1;

      if (zultra_huffman_encoder_write_raw_table(&tablesEncoder, NCODELENBITS /* code length bits */, nCodeLenSyms, pBitWriter) < 0)
         return -1;
      if (zultra_huffman_encoder_write_var_lengths(&tablesEncoder, nLiteralSyms + nOffsetSyms, nCodeLength, nBestCodesMask, pBitWriter) < 0)
         return -1;
   }

   /* Write compressed block data */
   return zultra_write_block_lwd(pCompressor, pBitWriter, pInWindow, nStartOffset, nStartOffset + nInDataSize);
}
