/*
 * bitwriter.h - variable bits writer implementation
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

#include "bitwriter.h"

/**
 * Initialize bit writer
 *
 * @param pOutData pointer to output buffer
 * @param nOutOffset current write index into output buffer
 * @param nMaxOutDataSize maximum size of output buffer, in bytes
 */
void zultra_bitwriter_init(zultra_bitwriter_t *pBitWriter, unsigned char *pOutData, int nOutOffset, const int nMaxOutDataSize) {
   pBitWriter->nEncBitCount = 0;
   pBitWriter->nEncBitsData = 0;
   pBitWriter->pOutData = pOutData;
   pBitWriter->nOutOffset = nOutOffset;
   pBitWriter->nMaxOutDataOffset = nMaxOutDataSize;
}

/**
 * Make a copy of a bit writer's state. This is used to save the current position and then rewind to it
 *
 * @param pDstBitWriter where to store bit writer state's copy
 * @param pSrcBitWriter bit writer state to copy
 */
void zultra_bitwriter_copy(zultra_bitwriter_t *pDstBitWriter, zultra_bitwriter_t *pSrcBitWriter) {
   pDstBitWriter->nEncBitCount = pSrcBitWriter->nEncBitCount;
   pDstBitWriter->nEncBitsData = pSrcBitWriter->nEncBitsData;
   pDstBitWriter->pOutData = pSrcBitWriter->pOutData;
   pDstBitWriter->nOutOffset = pSrcBitWriter->nOutOffset;
   pDstBitWriter->nMaxOutDataOffset = pSrcBitWriter->nMaxOutDataOffset;
}

/**
 * Write a number of bits out
 *
 * @param pBitWriter bit writer context
 * @param nValue value to write
 * @param nBits number of bits to write for value
 *
 * @return 0 for success, -1 for error
 */
int zultra_bitwriter_put_bits(zultra_bitwriter_t *pBitWriter, const unsigned int nValue, const int nBits) {
   if (nBits > 16) return -1;

   pBitWriter->nEncBitsData |= (nValue << pBitWriter->nEncBitCount);
   pBitWriter->nEncBitCount += nBits;

   while (pBitWriter->nEncBitCount >= 8) {
      if (pBitWriter->nOutOffset >= pBitWriter->nMaxOutDataOffset) return -1;
      pBitWriter->pOutData[pBitWriter->nOutOffset++] = (unsigned char)pBitWriter->nEncBitsData;
      pBitWriter->nEncBitsData >>= 8;
      pBitWriter->nEncBitCount -= 8;
   }

   return 0;
}

/**
 * Flush any pending bits out, to pad to a byte
 *
 * @param pBitWriter bit writer context
 *
 * @return 0 for success, -1 for error
 */
int zultra_bitwriter_flush_bits(zultra_bitwriter_t *pBitWriter) {
   if (pBitWriter->nEncBitCount > 8) return -1;

   if (pBitWriter->nEncBitCount > 0) {
      if (pBitWriter->nOutOffset >= pBitWriter->nMaxOutDataOffset) return -1;

      pBitWriter->pOutData[pBitWriter->nOutOffset++] = (unsigned char)(pBitWriter->nEncBitsData & ((1 << pBitWriter->nEncBitCount) - 1));
      pBitWriter->nEncBitsData = 0;
      pBitWriter->nEncBitCount = 0;
   }

   return 0;
}
