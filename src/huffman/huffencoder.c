/*
 * huffencoder.c - huffman encoder implementation
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

#include <stdlib.h>
#include <string.h>
#include "huffencoder.h"
#include "bitwriter.h"
#include "../format.h"

/** Code lengths table symbol ordering (RFC 1951 section 3.2.7) */
static const unsigned short zultra_huffman_encoder_codelen_sym_idx[NCODELENSYMS] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

/* Quicksort value[idx[n]] in ascending order, and also order idx[n] in ascending order if the values are equal. */

static void zultra_huffman_encoder_qsort(const int *value, int *idx, int left, int right) {
   int tmp;
   int i, last, mid = (left + right) >> 1;
   
   if (left >= right)
      return;

   tmp = idx[left]; 
   idx[left] = idx[mid]; 
   idx[mid] = tmp;
   last = left;
   
   for (i = left + 1; i <= right; i++) {
      tmp = idx[i];
      if (value[idx[left]] > value[tmp] || (value[idx[left]] == value[tmp] && idx[left] > tmp)) {
         last++;
         idx[i] = idx[last];
         idx[last] = tmp;
      }
   }

   tmp = idx[left];
   idx[left] = idx[last];
   idx[last] = tmp;

   zultra_huffman_encoder_qsort(value, idx, left, last - 1);
   zultra_huffman_encoder_qsort(value, idx, last + 1, right);
}

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
int zultra_huffman_encoder_init(zultra_huffman_encoder_t *pEncoder, const int nSymbols, const int nMaxCodeLength, const int nDefaultCodeLength) {
   int i;

   if (nSymbols < 0 || nSymbols > MAX_SYMBOLS || nMaxCodeLength < 0 || nMaxCodeLength > 32)
      return -1;

   pEncoder->nSymbols = nSymbols;
   pEncoder->nMaxCodeLength = nMaxCodeLength;

   for (i = 0; i < nSymbols; i++)
      pEncoder->nEntropy[i] = 0;
   while (i < MAX_SYMBOLS)
      pEncoder->nEntropy[i++] = 0;

   for (i = 0; i < nSymbols; i++)
      pEncoder->nCodeWord[i] = 0;
   while (i < MAX_SYMBOLS)
      pEncoder->nCodeWord[i++] = 0;

   for (i = 0; i < nSymbols; i++)
      pEncoder->nCodeLength[i] = nDefaultCodeLength;
   while (i < MAX_SYMBOLS)
      pEncoder->nCodeLength[i++] = 0;

   return 0;
}

/**
 * Build static canonical huffman codewords table
 *
 * @param pEncoder encoding context
 *
 * @return 0 for success, nonzero for error
 */
int zultra_huffman_encoder_build_static_codewords(zultra_huffman_encoder_t *pEncoder) {
   int nMinQueue[MAX_SYMBOLS];
   int nNumSorted = 0;
   int i;

   /* Enumerate all symbols as they all have code lengths in the static tables */

   for (i = 0; i < pEncoder->nSymbols; i++) {
      nMinQueue[nNumSorted++] = i;
   }

   /* Issue canonical huffman codewords */

   if (nNumSorted > 0) {
      /* Sort symbols of the same code length, alphabetically */
      zultra_huffman_encoder_qsort(pEncoder->nCodeLength, nMinQueue, 0, nNumSorted - 1);

      unsigned int nCanonicalCodeWord = 0;
      int nCanonicalLength = pEncoder->nCodeLength[nMinQueue[0]];

      for (i = 0; i < nNumSorted; i++) {
         int n = nMinQueue[i];
         unsigned int nRevWord;

         /* Get upside down codeword (branchless method by Eric Biggers) */
         nRevWord = ((nCanonicalCodeWord & 0x5555) << 1) | ((nCanonicalCodeWord & 0xaaaa) >> 1);
         nRevWord = ((nRevWord & 0x3333) << 2) | ((nRevWord & 0xcccc) >> 2);
         nRevWord = ((nRevWord & 0x0f0f) << 4) | ((nRevWord & 0xf0f0) >> 4);
         nRevWord = ((nRevWord & 0x00ff) << 8) | ((nRevWord & 0xff00) >> 8);
         nRevWord = nRevWord >> (16 - nCanonicalLength);

         pEncoder->nCodeWord[n] = nRevWord;
         if ((i + 1) < nNumSorted) {
            int nNewLength = pEncoder->nCodeLength[nMinQueue[i + 1]];
            nCanonicalCodeWord = (nCanonicalCodeWord + 1) << (nNewLength - nCanonicalLength);
            nCanonicalLength = nNewLength;
         }
      }
   }

   return 0;
}

/**
 * Estimate dynamic canonical huffman codewords lengths
 *
 * @param pEncoder encoding context
 *
 * @return 0 for success, nonzero for error
 */
int zultra_huffman_encoder_estimate_dynamic_codelens(zultra_huffman_encoder_t *pEncoder) {
   int nMinQueue[MAX_SYMBOLS];
   int A[MAX_SYMBOLS];
   int nNumSorted = 0;
   int i;

   if (pEncoder->nSymbols < 0 || pEncoder->nSymbols > MAX_SYMBOLS)
      return -1;

   /* Enumerate symbols with a nonzero entropy */
   for (i = 0; i < pEncoder->nSymbols; i++) {
      if (pEncoder->nEntropy[i])
         nMinQueue[nNumSorted++] = i;
   }
   for (; i < MAX_SYMBOLS; i++)
      nMinQueue[i] = 0;

   if (nNumSorted > 1) {
      /* Sort symbols (indices) by increasing entropy */
      zultra_huffman_encoder_qsort(pEncoder->nEntropy, nMinQueue, 0, nNumSorted - 1);

      /* Put sorted entropies in A[] for code length building */
      for (i = 0; i < nNumSorted; i++) {
         A[i] = pEncoder->nEntropy[nMinQueue[i]];
      }

      /**
       * Build huffman code lengths using the Moffat-Katajainen method ("In-Place Calculation of Minimum-Redundancy Codes")
       * http://hjemmesider.diku.dk/~jyrki/Paper/WADS95.pdf
       *
       * This is a straight implementation from the paper, except that A[], the array of entropies that becomes the array of code lengths,
       * is 0-based instead of 1-based as in the paper.
       *
       * As we only need the code lengths and not the codewords (which will be issued canonically), this method is a lot faster than
       * the tree-based Huffman algorithm.
       */

      const int n = nNumSorted;
      int s = 0, r = 0;
      int t;

      /* Phase 1 */

      for (t = 0; t < (n - 1); t++) {
         int nNew;

         if (s >= n || (r < t && A[r] < A[s])) {
            /* Select an internal tree node */
            nNew = A[r];
            A[r] = t + 1;
            r++;
         }
         else {
            /* Select a singleton leaf node */
            nNew = A[s];
            s++;
         }

         if (s >= n || (r < t && A[r] < A[s])) {
            /* Select an internal tree node */
            nNew += A[r];
            A[r] = t + 1;
            r++;
         }
         else {
            /* Select a singleton leaf node */
            nNew += A[s];
            s++;
         }

         A[t] = nNew;
      }

      /* Phase 2 */

      A[n - 2] = 0;
      for (t = n - 3; t >= 0; t--)
         A[t] = A[A[t] - 1] + 1;

      int a = 1;
      int u = 0;
      int d = 0;
      int x = n - 1;

      t = n - 2;
      while (a > 0) {
         while (t >= 0 && A[t] == d) {
            u++;
            t--;
         }
         while (a > u) {
            A[x] = d;
            x--;
            a--;
         }
         a = u << 1;
         d++;
         u = 0;
      }

      /* Extract code lengths using the array of symbol indices originally sorted by entropy */
      memset(pEncoder->nCodeLength, 0, MAX_SYMBOLS * sizeof(int));
      for (i = 0; i < nNumSorted; i++) {
         pEncoder->nCodeLength[nMinQueue[i]] = A[i];
      }
   }
   else {
      /* Zero or one symbols; set up a single codelength of one bit */
      memset(pEncoder->nCodeLength, 0, MAX_SYMBOLS * sizeof(int));
      pEncoder->nCodeLength[0] = 1;
   }

   return 0;
}

/**
 * Build canonical huffman codewords table
 *
 * @param pEncoder encoding context
 *
 * @return 0 for success, nonzero for error
 */
int zultra_huffman_encoder_build_dynamic_codewords(zultra_huffman_encoder_t *pEncoder) {
   int nMinQueue[MAX_SYMBOLS];
   int nNumSorted = 0;
   int i;

   /* Build codeword lengths */
   if (zultra_huffman_encoder_estimate_dynamic_codelens(pEncoder) < 0)
      return -1;

   /* Enumerate symbols with a non-zero codeword length */
   nNumSorted = 0;
   for (i = 0; i < pEncoder->nSymbols; i++) {
      if (pEncoder->nCodeLength[i]) {
         nMinQueue[nNumSorted++] = i;
      }
   }

   if (nNumSorted > 0 && pEncoder->nMaxCodeLength > 0) {
      /* Sort symbols of the same code length, alphabetically. This is so that we can generate canonical codewords and know the order
       * of the symbols within each length, and only then just need to provide the code lengths to the decompressor. See Huffman
       * canonical codes https://en.wikipedia.org/wiki/Canonical_Huffman_code */

      zultra_huffman_encoder_qsort(pEncoder->nCodeLength, nMinQueue, 0, nNumSorted - 1);

      if (pEncoder->nCodeLength[nMinQueue[nNumSorted - 1]] > pEncoder->nMaxCodeLength) {
         /**
          * Limit maximum codeword length and propagate error backward
          * See http://cbloomrants.blogspot.com/2010/07/07-03-10-length-limitted-huffman-codes.html
          * and also https://github.com/glinscott/linzip2/blob/master/main.cc which is good inspiration but doesn't actually produce complete codes, which zlib requires
          */

         int k = 0;
         int maxk = 1 << pEncoder->nMaxCodeLength;
         for (i = nNumSorted - 1; i >= 0; i--) {
            int n = nMinQueue[i];

            /* Limit any code length to nMaxCodeLength */
            if (pEncoder->nCodeLength[n] > pEncoder->nMaxCodeLength)
               pEncoder->nCodeLength[n] = pEncoder->nMaxCodeLength;

            /* Add up the kraft number */
            k += maxk >> pEncoder->nCodeLength[n];
         }

         /* Propagate error backward */
         for (i = nNumSorted - 1; k > maxk && i >= 0; i--) {
            int n = nMinQueue[i];

            while (pEncoder->nCodeLength[n] < pEncoder->nMaxCodeLength && k > maxk) {
               pEncoder->nCodeLength[n]++;
               k -= maxk >> pEncoder->nCodeLength[n];
            }
         }

         /* If we have room to reduce some symbol codeword lengths again, do it */
         for (i = 0; k < maxk && i <= nNumSorted; i++) {
            int n = nMinQueue[i];

            while ((k + (maxk >> pEncoder->nCodeLength[n])) <= maxk) {
               k += maxk >> pEncoder->nCodeLength[n];
               pEncoder->nCodeLength[n]--;
            }
         }

         /* The error propagation may have messed up the order of symbols with identical code lengths, sort them again */
         zultra_huffman_encoder_qsort(pEncoder->nCodeLength, nMinQueue, 0, nNumSorted - 1);
      }
   }

   /* Issue canonical huffman codewords */

   if (nNumSorted > 0) {
      unsigned int nCanonicalCodeWord = 0;
      int nCanonicalLength = pEncoder->nCodeLength[nMinQueue[0]];

      for (i = 0; i < nNumSorted; i++) {
         int n = nMinQueue[i];
         unsigned int nRevWord;

         /* Get upside down codeword (branchless method by Eric Biggers) */
         nRevWord = ((nCanonicalCodeWord & 0x5555) << 1) | ((nCanonicalCodeWord & 0xaaaa) >> 1);
         nRevWord = ((nRevWord & 0x3333) << 2) | ((nRevWord & 0xcccc) >> 2);
         nRevWord = ((nRevWord & 0x0f0f) << 4) | ((nRevWord & 0xf0f0) >> 4);
         nRevWord = ((nRevWord & 0x00ff) << 8) | ((nRevWord & 0xff00) >> 8);
         nRevWord = nRevWord >> (16 - nCanonicalLength);

         pEncoder->nCodeWord[n] = nRevWord;
         if ((i + 1) < nNumSorted) {
            int nNewLength = pEncoder->nCodeLength[nMinQueue[i + 1]];
            nCanonicalCodeWord = (nCanonicalCodeWord + 1) << (nNewLength - nCanonicalLength);
            nCanonicalLength = nNewLength;
         }
      }
   }

   return 0;
}

/**
 * Write codeword for symbol
 *
 * @param pEncoder encoding context
 * @param nSymbol symbol to write codeword of
 * @param pBitWriter bit writer context
 *
 * @return 0 for success, -1 for error
 */
int zultra_huffman_encoder_write_codeword(zultra_huffman_encoder_t *pEncoder, const int nSymbol, zultra_bitwriter_t *pBitWriter) {
   if (nSymbol >= 0 && nSymbol < pEncoder->nSymbols)
      return zultra_bitwriter_put_bits(pBitWriter, pEncoder->nCodeWord[nSymbol], pEncoder->nCodeLength[nSymbol]);
   else
      return -1;
}

/**
 * Get number of symbols in fixed code length huffman table
 *
 * @param pEncoder encoding context
 *
 * @return number of symbols that actually need to be written
 */
int zultra_huffman_encoder_get_raw_table_size(zultra_huffman_encoder_t *pEncoder) {
   int i = pEncoder->nSymbols;

   while (i > 4 && !pEncoder->nCodeLength[zultra_huffman_encoder_codelen_sym_idx[i - 1]])
      i--;
   return i;
}

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
int zultra_huffman_encoder_write_raw_table(zultra_huffman_encoder_t *pEncoder, const int nLenBits, const int nWriteSymbols, zultra_bitwriter_t *pBitWriter) {
   int i;
   
   if (nWriteSymbols < 4 || nWriteSymbols > pEncoder->nSymbols)
      return -1;

   if (zultra_bitwriter_get_offset(pBitWriter) < 0)
      return -1;

   i = 0;
   while (i < nWriteSymbols) {
      zultra_bitwriter_put_bits(pBitWriter, pEncoder->nCodeLength[zultra_huffman_encoder_codelen_sym_idx[i]], nLenBits);
      i++;

      if (zultra_bitwriter_get_offset(pBitWriter) < 0)
         return -1;
   }

   return 0;
}

/**
 * Update code lengths encoder to include codes for an array of symbol codelengths
 *
 * @param pTablesEncoder encoding context for code lengths
 * @param nWriteSymbols number of symbols whose codewords are to be included
 * @param pCodeLength array of symbols codelengths
 */
void zultra_huffman_encoder_update_var_lengths_entropy(zultra_huffman_encoder_t *pTablesEncoder, const int nWriteSymbols, const int *pCodeLength, const unsigned int nEnabledCodesMask) {
   int i;

   i = 0;
   while (i < nWriteSymbols) {
      int nRunLen = 1;
      while ((i + nRunLen) < nWriteSymbols && pCodeLength[i + nRunLen] == pCodeLength[i])
         nRunLen++;

      if (pCodeLength[i] == 0) {
         if (nRunLen >= 3) {
            while (nRunLen >= 11 && (nEnabledCodesMask & 4)) {
               int nMaxRunLen = nRunLen;
               if (nMaxRunLen > 138) nMaxRunLen = 138;
               pTablesEncoder->nEntropy[18]++;
               nRunLen -= nMaxRunLen;
               i += nMaxRunLen;
            }
            while (nRunLen >= 3 && (nEnabledCodesMask & 2)) {
               int nMaxRunLen = nRunLen;
               if (nMaxRunLen > 10) nMaxRunLen = 10;
               pTablesEncoder->nEntropy[17]++;
               nRunLen -= nMaxRunLen;
               i += nMaxRunLen;
            }

            if (nRunLen) {
               nRunLen--;

               pTablesEncoder->nEntropy[pCodeLength[i]]++;
               i++;
            }
         }
         else {
            nRunLen--;

            pTablesEncoder->nEntropy[pCodeLength[i]]++;
            i++;
         }
      }
      else {
         nRunLen--;

         int nCodeLength = pCodeLength[i];
         if (nCodeLength > 15) nCodeLength = 15;
         pTablesEncoder->nEntropy[nCodeLength]++;
         i++;

         if (nRunLen == 7 && (nEnabledCodesMask & 1) && (nEnabledCodesMask & 8) == 0) {
            pTablesEncoder->nEntropy[16]++;
            nRunLen -= 4;
            i += 4;

            pTablesEncoder->nEntropy[16]++;
            nRunLen -= 3;
            i += 3;
         }
         else if (nRunLen == 8 && (nEnabledCodesMask & 1) && (nEnabledCodesMask & 16) == 0) {
            pTablesEncoder->nEntropy[16]++;
            nRunLen -= 4;
            i += 4;

            pTablesEncoder->nEntropy[16]++;
            nRunLen -= 4;
            i += 4;
         }

         while (nRunLen >= 3 && (nEnabledCodesMask & 1)) {
            int nMaxRunLen = nRunLen;
            if (nMaxRunLen > 6) nMaxRunLen = 6;
            pTablesEncoder->nEntropy[16]++;
            nRunLen -= nMaxRunLen;
            i += nMaxRunLen;
         }
      }
   }
}

/**
 * Get number of defined symbols in huffman table
 *
 * @param pEncoder encoding context
 * @param nMinSymbols minimum number of symbols to report
 *
 * @return number of symbols that actually need to be written
 */
int zultra_huffman_encoder_get_defined_var_lengths_count(zultra_huffman_encoder_t *pEncoder, const int nMinSymbols) {
   int i = pEncoder->nSymbols;

   while (i > nMinSymbols && !pEncoder->nCodeLength[i - 1])
      i--;
   return i;
}

/**
 * Get cost of encoding an array of symbol codelengths using the code lengths encoder, in bits
 *
 * @param pTablesEncoder encoding context for code lengths
 * @param nWriteSymbols number of symbols to account for
 * @param pCodeLength array of symbols codelengths
 *
 * @return cost in bits
 */
int zultra_huffman_encoder_get_var_lengths_size(zultra_huffman_encoder_t *pTablesEncoder, const int nWriteSymbols, const int *pCodeLength, const unsigned int nEnabledCodesMask) {
   int i;
   int nBitSize = 0;

   i = 0;
   while (i < nWriteSymbols) {
      int nRunLen = 1;
      while ((i + nRunLen) < nWriteSymbols && pCodeLength[i + nRunLen] == pCodeLength[i])
         nRunLen++;

      if (pCodeLength[i] == 0) {
         if (nRunLen >= 3) {
            while (nRunLen >= 11 && (nEnabledCodesMask & 4)) {
               int nMaxRunLen = nRunLen;
               if (nMaxRunLen > 138) nMaxRunLen = 138;
               nBitSize += pTablesEncoder->nCodeLength[18] + 7;
               nRunLen -= nMaxRunLen;
               i += nMaxRunLen;
            }
            while (nRunLen >= 3 && (nEnabledCodesMask & 2)) {
               int nMaxRunLen = nRunLen;
               if (nMaxRunLen > 10) nMaxRunLen = 10;
               nBitSize += pTablesEncoder->nCodeLength[17] + 3;
               nRunLen -= nMaxRunLen;
               i += nMaxRunLen;
            }

            if (nRunLen) {
               nRunLen--;

               nBitSize += pTablesEncoder->nCodeLength[pCodeLength[i]];
               i++;
            }
         }
         else {
            nRunLen--;

            nBitSize += pTablesEncoder->nCodeLength[pCodeLength[i]];
            i++;
         }
      }
      else {
         nRunLen--;

         int nCodeLength = pCodeLength[i];
         if (nCodeLength > 15) nCodeLength = 15;
         nBitSize += pTablesEncoder->nCodeLength[nCodeLength];
         i++;

         if (nRunLen == 7 && (nEnabledCodesMask & 1) && (nEnabledCodesMask & 8) == 0) {
            nBitSize += pTablesEncoder->nCodeLength[16] + 2;
            nRunLen -= 4;
            i += 4;

            nBitSize += pTablesEncoder->nCodeLength[16] + 2;
            nRunLen -= 3;
            i += 3;
         }
         else if (nRunLen == 8 && (nEnabledCodesMask & 1) && (nEnabledCodesMask & 16) == 0) {
            nBitSize += pTablesEncoder->nCodeLength[16] + 2;
            nRunLen -= 4;
            i += 4;

            nBitSize += pTablesEncoder->nCodeLength[16] + 2;
            nRunLen -= 4;
            i += 4;
         }

         while (nRunLen >= 3 && (nEnabledCodesMask & 1)) {
            int nMaxRunLen = nRunLen;
            if (nMaxRunLen > 6) nMaxRunLen = 6;
            nBitSize += pTablesEncoder->nCodeLength[16] + 2;
            nRunLen -= nMaxRunLen;
            i += nMaxRunLen;
         }
      }
   }

   return nBitSize;
}

/**
 * Write an array of symbol codelengths using the code lengths encoder
 *
 * @param pTablesEncoder encoding context for code lengths
 * @param nWriteSymbols number of symbols to actually write
 * @param pCodeLength array of symbols codelengths
 * @param pBitWriter bit writer context
 *
 * @return 0 for success, -1 for error
 */
int zultra_huffman_encoder_write_var_lengths(zultra_huffman_encoder_t *pTablesEncoder, const int nWriteSymbols, const int *pCodeLength, const unsigned int nEnabledCodesMask, zultra_bitwriter_t *pBitWriter) {
   int i;

   if (zultra_bitwriter_get_offset(pBitWriter) < 0)
      return -1;

   i = 0;
   while (i < nWriteSymbols) {
      int nRunLen = 1;
      while ((i + nRunLen) < nWriteSymbols && pCodeLength[i + nRunLen] == pCodeLength[i])
         nRunLen++;

      if (pCodeLength[i] == 0) {
         if (nRunLen >= 3) {
            while (nRunLen >= 11 && (nEnabledCodesMask & 4)) {
               int nMaxRunLen = nRunLen;
               if (nMaxRunLen > 138) nMaxRunLen = 138;
               zultra_huffman_encoder_write_codeword(pTablesEncoder, 18, pBitWriter);
               zultra_bitwriter_put_bits(pBitWriter, nMaxRunLen - 11, 7);
               nRunLen -= nMaxRunLen;
               i += nMaxRunLen;
            }
            while (nRunLen >= 3 && (nEnabledCodesMask & 2)) {
               int nMaxRunLen = nRunLen;
               if (nMaxRunLen > 10) nMaxRunLen = 10;
               zultra_huffman_encoder_write_codeword(pTablesEncoder, 17, pBitWriter);
               zultra_bitwriter_put_bits(pBitWriter, nMaxRunLen - 3, 3);
               nRunLen -= nMaxRunLen;
               i += nMaxRunLen;
            }

            if (nRunLen) {
               nRunLen--;

               zultra_huffman_encoder_write_codeword(pTablesEncoder, pCodeLength[i], pBitWriter);
               i++;
            }

            if (zultra_bitwriter_get_offset(pBitWriter) < 0)
               return -1;
         }
         else {
            nRunLen--;

            zultra_huffman_encoder_write_codeword(pTablesEncoder, pCodeLength[i], pBitWriter);
            i++;
         }
      }
      else {
         nRunLen--;

         int nCodeLength = pCodeLength[i];
         if (nCodeLength > 15) return -1;

         zultra_huffman_encoder_write_codeword(pTablesEncoder, nCodeLength, pBitWriter);
         i++;

         if (nRunLen == 7 && (nEnabledCodesMask & 1) && (nEnabledCodesMask & 8) == 0) {
            zultra_huffman_encoder_write_codeword(pTablesEncoder, 16, pBitWriter);
            zultra_bitwriter_put_bits(pBitWriter, 4 - 3, 2);
            nRunLen -= 4;
            i += 4;

            zultra_huffman_encoder_write_codeword(pTablesEncoder, 16, pBitWriter);
            zultra_bitwriter_put_bits(pBitWriter, 3 - 3, 2);
            nRunLen -= 3;
            i += 3;
         }
         else if (nRunLen == 8 && (nEnabledCodesMask & 1) && (nEnabledCodesMask & 16) == 0) {
            zultra_huffman_encoder_write_codeword(pTablesEncoder, 16, pBitWriter);
            zultra_bitwriter_put_bits(pBitWriter, 4 - 3, 2);
            nRunLen -= 4;
            i += 4;

            zultra_huffman_encoder_write_codeword(pTablesEncoder, 16, pBitWriter);
            zultra_bitwriter_put_bits(pBitWriter, 4 - 3, 2);
            nRunLen -= 4;
            i += 4;
         }

         while (nRunLen >= 3 && (nEnabledCodesMask & 1)) {
            int nMaxRunLen = nRunLen;
            if (nMaxRunLen > 6) nMaxRunLen = 6;
            zultra_huffman_encoder_write_codeword(pTablesEncoder, 16, pBitWriter);
            zultra_bitwriter_put_bits(pBitWriter, nMaxRunLen - 3, 2);
            nRunLen -= nMaxRunLen;
            i += nMaxRunLen;
         }

         if (zultra_bitwriter_get_offset(pBitWriter) < 0)
            return -1;
      }
   }

   return 0;
}
