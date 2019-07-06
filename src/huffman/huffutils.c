/*
Copyright 2011 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Author: lode.vandevenne@gmail.com (Lode Vandevenne)
Author: jyrki.alakuijala@gmail.com (Jyrki Alakuijala)
*/

#include "huffutils.h"

static inline size_t AbsDiff(size_t x, size_t y) {
  if (x > y)
    return x - y;
  else
    return y - x;
}

/*
Changes the population counts in a way that the consequent Huffman tree
compression, especially its rle-part, will be more likely to compress this data
more efficiently. length contains the size of the histogram.
*/
void zultra_huffman_encoder_optimize_for_rle(int length, int* counts, int* good_for_rle) {
  int i, k, stride;
  size_t symbol, sum, limit;

  /* 1) We don't want to touch the trailing zeros. We may break the
  rules of the format by adding more data in the distance codes. */
  for (; length >= 0; --length) {
    if (length == 0) {
      return;
    }
    if (counts[length - 1] != 0) {
      /* Now counts[0..length - 1] does not have trailing zeros. */
      break;
    }
  }
  /* 2) Let's mark all population counts that already can be encoded
  with an rle code.*/
  for (i = 0; i < length; ++i) good_for_rle[i] = 0;

  /* Let's not spoil any of the existing good rle codes.
  Mark any seq of 0's that is longer than 5 as a good_for_rle.
  Mark any seq of non-0's that is longer than 7 as a good_for_rle.*/
  symbol = counts[0];
  stride = 0;
  for (i = 0; i < length + 1; ++i) {
    if (i == length || counts[i] != symbol) {
      if ((symbol == 0 && stride >= 5) || (symbol != 0 && stride >= 7)) {
        for (k = 0; k < stride; ++k) {
          good_for_rle[i - k - 1] = 1;
        }
      }
      stride = 1;
      if (i != length) {
        symbol = counts[i];
      }
    } else {
      ++stride;
    }
  }

  /* 3) Let's replace those population counts that lead to more rle codes. */
  stride = 0;
  limit = counts[0];
  sum = 0;
  for (i = 0; i < length + 1; ++i) {
    if (i == length || good_for_rle[i]
        /* Heuristic for selecting the stride ranges to collapse. */
        || AbsDiff(counts[i], limit) >= 4) {
      if (stride >= 4 || (stride >= 3 && sum == 0)) {
        /* The stride must end, collapse what we have, if we have enough (4). */
        int count = (int)((sum + stride / 2) / stride);
        if (count < 1) count = 1;
        if (sum == 0) {
          /* Don't make an all zeros stride to be upgraded to ones. */
          count = 0;
        }
        for (k = 0; k < stride; ++k) {
          /* We don't want to change value at counts[i],
          that is already belonging to the next stride. Thus - 1. */
          counts[i - k - 1] = count;
        }
      }
      stride = 0;
      sum = 0;
      if (i < length - 3) {
        /* All interesting strides have a count of at least 4,
        at least when non-zeros. */
        limit = (counts[i] + counts[i + 1] +
                 counts[i + 2] + counts[i + 3] + 2) / 4;
      } else if (i < length) {
        limit = counts[i];
      } else {
        limit = 0;
      }
    }
    ++stride;
    if (i != length) {
      sum += counts[i];
    }
  }
}
