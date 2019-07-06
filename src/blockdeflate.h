/*
 * blockdeflate.h - optimal DEFLATE block compressor definitions
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

#ifndef _BLOCKDEFLATE_H
#define _BLOCKDEFLATE_H

/* Forward declarations */
typedef struct _zultra_bitwriter_s zultra_bitwriter_t;
typedef struct _zultra_huffman_encoder_s zultra_huffman_encoder_t;
typedef struct _zultra_compressor_s zultra_compressor_t;

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
int zultra_block_prepare_cost_evaluation(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nInDataSize);

/**
 * Estimate compressed size of one parsed block of data using static huffman tables
 *
 * @param pLiteralsEncoder literals encoding context
 * @param pOffsetEncoder offset encoding context
 * @param pStaticCost where to store the compressed cost in bits when using a static huffman table
 *
 * @return 0 for success, -1 in case of an error
 */
int zultra_block_evaluate_static_cost(zultra_huffman_encoder_t *pLiteralsEncoder, zultra_huffman_encoder_t *pOffsetEncoder, int *pStaticCost);

/**
 * Estimate compressed size of one parsed block of data using dynamic huffman tables
 *
 * @param pLiteralsEncoder literals encoding context
 * @param pOffsetEncoder offset encoding context
 * @param pDynamicCost where to store the compressed cost in bits when using a dynamic huffman table
 *
 * @return 0 for success, -1 in case of an error
 */
int zultra_block_evaluate_dynamic_cost(zultra_huffman_encoder_t *pLiteralsEncoder, zultra_huffman_encoder_t *pOffsetEncoder, int *pDynamicCost);

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
   const int nMaxSplits, int *pSplitOffset);

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
 * @return 0 if the data was compressed successfully (and the bit writer state was updated), or -1 if the data is uncompressible
 */
int zultra_block_deflate(zultra_compressor_t *pCompressor, zultra_bitwriter_t *pBitWriter, const unsigned char *pInWindow, const int nStartOffset, const int nInDataSize, const int nIsDynamic);

#endif /* _BLOCKDEFLATE_H */
