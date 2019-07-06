/*
 * huffencoder.h - huffman encoder definitions
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

#ifndef _HUFF_ENCODER_H
#define _HUFF_ENCODER_H

#include "bitwriter.h"

/** Maximum number of symbols that we need to generate huffman codewords for */
#define MAX_SYMBOLS 288

#define MAX_CODES_MASK     31

/** Huffman encoding context */
typedef struct _zultra_huffman_encoder_s {
   int nSymbols;
   int nMaxCodeLength;

   int nEntropy[MAX_SYMBOLS];

   unsigned int nCodeWord[MAX_SYMBOLS];
   int nCodeLength[MAX_SYMBOLS];
} zultra_huffman_encoder_t;

/**
 * Initialize huffman encoder
 *
 * @param pEncoder encoding context
 * @param nSymbols number of symbols to build codes for
 * @param nMaxCodeLength maximum number of bits to use for a codeword
 * @param nDefaultCodeLength default number of bits/codeword, until the codewords table is built
 *
 * @return 0 for success, nonzero for error
 */
int zultra_huffman_encoder_init(zultra_huffman_encoder_t *pEncoder, const int nSymbols, const int nMaxCodeLength, const int nDefaultCodeLength);

/**
 * Build static canonical huffman codewords table
 *
 * @param pEncoder encoding context
 *
 * @return 0 for success, nonzero for error
 */
int zultra_huffman_encoder_build_static_codewords(zultra_huffman_encoder_t *pEncoder);

/**
 * Estimate dynamic canonical huffman codewords lengths
 *
 * @param pEncoder encoding context
 *
 * @return 0 for success, nonzero for error
 */
int zultra_huffman_encoder_estimate_dynamic_codelens(zultra_huffman_encoder_t *pEncoder);

/**
 * Build dynamic canonical huffman codewords table
 *
 * @param pEncoder encoding context
 *
 * @return 0 for success, nonzero for error
 */
int zultra_huffman_encoder_build_dynamic_codewords(zultra_huffman_encoder_t *pEncoder);

/**
 * Write codeword for symbol
 *
 * @param pEncoder encoding context
 * @param nSymbol symbol to write codeword of
 * @param pBitWriter bit writer context
 *
 * @return 0 for success, -1 for error
 */
int zultra_huffman_encoder_write_codeword(zultra_huffman_encoder_t *pEncoder, const int nSymbol, zultra_bitwriter_t *pBitWriter);

/**
 * Get number of symbols in fixed code length huffman table
 *
 * @param pEncoder encoding context
 *
 * @return number of symbols that actually need to be written
 */
int zultra_huffman_encoder_get_raw_table_size(zultra_huffman_encoder_t *pEncoder);

/**
 * Encode huffman table using fixed code length bit sizes
 *
 * @param pEncoder encoding context to write table for
 * @param nLenBits number of bits per code length
 * @param nWriteSymbols number of symbols to actually write
 * @param pBitWriter bit writer context
 *
 * @return 0 for success, -1 for error
 */
int zultra_huffman_encoder_write_raw_table(zultra_huffman_encoder_t *pEncoder, const int nLenBits, const int nWriteSymbols, zultra_bitwriter_t *pBitWriter);

/**
 * Update code lengths encoder to include codes for an array of symbol codelengths
 *
 * @param pTablesEncoder encoding context for code lengths
 * @param nWriteSymbols number of symbols whose codewords are to be included
 * @param pCodeLength array of symbols codelengths
 * @param nEnabledCodesMask bitmask of enabled RLE codes
 */
void zultra_huffman_encoder_update_var_lengths_entropy(zultra_huffman_encoder_t *pTablesEncoder, const int nWriteSymbols, const int *pCodeLength, const unsigned int nEnabledCodesMask);

/**
 * Get number of defined symbols in huffman table
 *
 * @param pEncoder encoding context
 * @param nMinSymbols minimum number of symbols to report
 *
 * @return number of symbols that actually need to be written
 */
int zultra_huffman_encoder_get_defined_var_lengths_count(zultra_huffman_encoder_t *pEncoder, const int nMinSymbols);

/**
 * Get cost of encoding an array of symbol codelengths using the code lengths encoder, in bits
 *
 * @param pTablesEncoder encoding context for code lengths
 * @param nWriteSymbols number of symbols to account for
 * @param pCodeLength array of symbols codelengths
 * @param nEnabledCodesMask bitmask of enabled RLE codes
 *
 * @return cost in bits
 */
int zultra_huffman_encoder_get_var_lengths_size(zultra_huffman_encoder_t *pTablesEncoder, const int nWriteSymbols, const int *pCodeLength, const unsigned int nEnabledCodesMask);

/**
 * Write an array of symbol codelengths using the code lengths encoder
 *
 * @param pTablesEncoder encoding context for code lengths
 * @param nWriteSymbols number of symbols to actually write
 * @param pCodeLength array of symbols codelengths
 * @param nEnabledCodesMask bitmask of enabled RLE codes
 * @param pBitWriter bit writer context
 *
 * @return 0 for success, -1 for error
 */
int zultra_huffman_encoder_write_var_lengths(zultra_huffman_encoder_t *pTablesEncoder, const int nWriteSymbols, const int *pCodeLength, const unsigned int nEnabledCodesMask, zultra_bitwriter_t *pBitWriter);

#endif /* _HUFF_ENCODER_H */
