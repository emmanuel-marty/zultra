/*
 * matchfinder.c - LZ match finder implementation
 *
 * The following copying information applies to this specific source code file:
 *
 * Written in 2019 by Emmanuel Marty <marty.emmanuel@gmail.com>
 * Portions written in 2014-2015 by Eric Biggers <ebiggers3@gmail.com>
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide via the Creative Commons Zero 1.0 Universal Public Domain
 * Dedication (the "CC0").
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the CC0 for more details.
 *
 * You should have received a copy of the CC0 along with this software; if not
 * see <http://creativecommons.org/publicdomain/zero/1.0/>.
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
#include "format.h"
#include "matchfinder.h"
#include "private.h"

/**
 * Parse input data, build suffix array and overlaid data structures to speed up match finding
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nInWindowSize total input size in bytes (previously compressed bytes + bytes to compress)
 *
 * @return 0 for success, non-zero for failure
 */
int zultra_build_suffix_array(zultra_compressor_t *pCompressor, const unsigned char *pInWindow, const int nInWindowSize) {
   unsigned int *intervals = pCompressor->intervals;

   /* Build suffix array from input data */
   if (divsufsort_build_array(&pCompressor->divsufsort_context, pInWindow, (saidx_t*)intervals, nInWindowSize) != 0) {
      return 100;
   }

   int *PLCP = (int*)pCompressor->pos_data;  /* Use temporarily */
   int *Phi = PLCP;
   int nCurLen = 0;
   int i;

   /* Compute the permuted LCP first (Kärkkäinen method) */
   Phi[intervals[0]] = -1;
   for (i = 1; i < nInWindowSize; i++)
      Phi[intervals[i]] = intervals[i - 1];
   for (i = 0; i < nInWindowSize; i++) {
      if (Phi[i] == -1) {
         PLCP[i] = 0;
         continue;
      }
      int nMaxLen = (i > Phi[i]) ? (nInWindowSize - i) : (nInWindowSize - Phi[i]);
      while (nCurLen < nMaxLen && pInWindow[i + nCurLen] == pInWindow[Phi[i] + nCurLen]) nCurLen++;
      PLCP[i] = nCurLen;
      if (nCurLen > 0)
         nCurLen--;
   }

   /* Rotate permuted LCP into the LCP. This has better cache locality than the direct Kasai LCP method. This also
    * saves us from having to build the inverse suffix array index, as the LCP is calculated without it using this method,
    * and the interval builder below doesn't need it either. */
   intervals[0] &= POS_MASK;
   for (i = 1; i < nInWindowSize - 1; i++) {
      int nIndex = (int)(intervals[i] & POS_MASK);
      int nLen = PLCP[nIndex];
      if (nLen < MIN_MATCH_SIZE)
         nLen = 0;
      if (nLen > MAX_MATCH_SIZE)
         nLen = MAX_MATCH_SIZE;
      intervals[i] = ((unsigned int)nIndex) | (((unsigned int)nLen) << LCP_SHIFT);
   }
   if (i < nInWindowSize)
      intervals[i] &= POS_MASK;

   /**
    * Build intervals for finding matches
    *
    * Methodology and code fragment taken from wimlib (CC0 license):
    * https://wimlib.net/git/?p=wimlib;a=blob_plain;f=src/lcpit_matchfinder.c;h=a2d6a1e0cd95200d1f3a5464d8359d5736b14cbe;hb=HEAD
    */
   unsigned int * const SA_and_LCP = intervals;
   unsigned int *pos_data = pCompressor->pos_data;
   unsigned int next_interval_idx;
   unsigned int *top = pCompressor->open_intervals;
   unsigned int prev_pos = SA_and_LCP[0] & POS_MASK;

   *top = 0;
   intervals[0] = 0;
   next_interval_idx = 1;

   for (int r = 1; r < nInWindowSize; r++) {
      const unsigned int next_pos = SA_and_LCP[r] & POS_MASK;
      const unsigned int next_lcp = SA_and_LCP[r] & LCP_MASK;
      const unsigned int top_lcp = *top & LCP_MASK;

      if (next_lcp == top_lcp) {
         /* Continuing the deepest open interval  */
         pos_data[prev_pos] = *top;
      }
      else if (next_lcp > top_lcp) {
         /* Opening a new interval  */
         *++top = next_lcp | next_interval_idx++;
         pos_data[prev_pos] = *top;
      }
      else {
         /* Closing the deepest open interval  */
         pos_data[prev_pos] = *top;
         for (;;) {
            const unsigned int closed_interval_idx = *top-- & POS_MASK;
            const unsigned int superinterval_lcp = *top & LCP_MASK;

            if (next_lcp == superinterval_lcp) {
               /* Continuing the superinterval */
               intervals[closed_interval_idx] = *top;
               break;
            }
            else if (next_lcp > superinterval_lcp) {
               /* Creating a new interval that is a
                * superinterval of the one being
                * closed, but still a subinterval of
                * its superinterval  */
               *++top = next_lcp | next_interval_idx++;
               intervals[closed_interval_idx] = *top;
               break;
            }
            else {
               /* Also closing the superinterval  */
               intervals[closed_interval_idx] = *top;
            }
         }
      }
      prev_pos = next_pos;
   }

   /* Close any still-open intervals.  */
   pos_data[prev_pos] = *top;
   for (; top > pCompressor->open_intervals; top--)
      intervals[*top & POS_MASK] = *(top - 1);

   /* Success */
   return 0;
}

/**
 * Find matches at the specified offset in the input window
 *
 * @param pCompressor compression context
 * @param nOffset offset to find matches at, in the input window
 * @param pMatches pointer to returned matches
 * @param nMaxMatches maximum number of matches to return (0 for none)
 *
 * @return number of matches
 */
static int zultra_find_matches_at(zultra_compressor_t *pCompressor, const int nOffset, zultra_match_t *pMatches, const int nMaxMatches) {
   unsigned int *intervals = pCompressor->intervals;
   unsigned int *pos_data = pCompressor->pos_data;
   unsigned int ref;
   unsigned int super_ref;
   unsigned int match_pos;
   zultra_match_t *matchptr;

   /**
    * Find matches using intervals
    *
    * Taken from wimlib (CC0 license):
    * https://wimlib.net/git/?p=wimlib;a=blob_plain;f=src/lcpit_matchfinder.c;h=a2d6a1e0cd95200d1f3a5464d8359d5736b14cbe;hb=HEAD
    */

    /* Get the deepest lcp-interval containing the current suffix. */
   ref = pos_data[nOffset];

   pos_data[nOffset] = 0;

   /* Ascend until we reach a visited interval, the root, or a child of the
    * root.  Link unvisited intervals to the current suffix as we go.  */
   while ((super_ref = intervals[ref & POS_MASK]) & LCP_MASK) {
      intervals[ref & POS_MASK] = nOffset | VISITED_FLAG;
      ref = super_ref;
   }

   if (super_ref == 0) {
      /* In this case, the current interval may be any of:
       * (1) the root;
       * (2) an unvisited child of the root */

      if (ref != 0)  /* Not the root?  */
         intervals[ref & POS_MASK] = nOffset | VISITED_FLAG;
      return 0;
   }

   /* Ascend indirectly via pos_data[] links.  */
   match_pos = super_ref & EXCL_VISITED_MASK;
   matchptr = pMatches;
   for (;;) {
      while ((super_ref = pos_data[match_pos]) > ref)
         match_pos = intervals[super_ref & POS_MASK] & EXCL_VISITED_MASK;
      intervals[ref & POS_MASK] = nOffset | VISITED_FLAG;
      pos_data[match_pos] = ref;

      if ((matchptr - pMatches) < nMaxMatches) {
         int nMatchOffset = (int)(nOffset - match_pos);

         if (nMatchOffset <= MAX_OFFSET) {
            matchptr->length = (unsigned int)(ref >> LCP_SHIFT);
            matchptr->offset = (unsigned int)nMatchOffset;
            matchptr++;
         }
      }

      if (super_ref == 0)
         break;
      ref = super_ref;
      match_pos = intervals[ref & POS_MASK] & EXCL_VISITED_MASK;
   }

   return (int)(matchptr - pMatches);
}

/**
 * Skip previously compressed bytes
 *
 * @param pCompressor compression context
 * @param nStartOffset current offset in input window (typically 0)
 * @param nEndOffset offset to skip to in input window (typically the number of previously compressed bytes)
 */
void zultra_skip_matches(zultra_compressor_t *pCompressor, const int nStartOffset, const int nEndOffset) {
   zultra_match_t match;
   int i;

   /* Skipping still requires scanning for matches, as this also performs a lazy update of the intervals. However,
    * we don't store the matches. */
   for (i = nStartOffset; i < nEndOffset; i++) {
      zultra_find_matches_at(pCompressor, i, &match, 0);
   }
}

/**
 * Find all matches for the data to be compressed. Up to NMATCHES_PER_OFFSET matches are stored for each offset, for
 * the optimizer to look at.
 *
 * @param pCompressor compression context
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 */
void zultra_find_all_matches(zultra_compressor_t *pCompressor, const int nStartOffset, const int nEndOffset) {
   zultra_match_t *pMatch = pCompressor->match + (nStartOffset << MATCHES_PER_OFFSET_SHIFT);
   int i;

   for (i = nStartOffset; i < nEndOffset; i++) {
      int nMatches = zultra_find_matches_at(pCompressor, i, pMatch, NMATCHES_PER_OFFSET);
      int m;

      for (m = 0; m < NMATCHES_PER_OFFSET; m++) {
         if (nMatches <= m || i > (nEndOffset - LAST_MATCH_OFFSET)) {
            pMatch->length = 0;
            pMatch->offset = 0;
         }
         else {
            int nMaxLen = (nEndOffset - LAST_LITERALS) - i;
            if (nMaxLen < 0)
               nMaxLen = 0;
            if (pMatch->length > (unsigned int)nMaxLen)
               pMatch->length = (unsigned int)nMaxLen;
         }

         pMatch++;
      }
   }
}
