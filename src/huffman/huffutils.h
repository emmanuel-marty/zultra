/*
 * huffutils.h - huffman utility functions
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

#ifndef _HUFF_UTILITY_H
#define _HUFF_UTILITY_H

#include <stdlib.h>

/**
 * Attempt to change the entropy so that the code lengths encode better with the code tables' RLE scheme.
 * 
 * @param length number of symbols in entropy table
 * @param counts array of entropy for each symbol
 * @param good_for_rle temporary storage space, array of length entries
 */
void zultra_huffman_encoder_optimize_for_rle(int length, int* counts, int* good_for_rle);

#endif /* _HUFF_UTILS_H */
