/*
 * format.h - deflate byte stream format definitions
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
 * Uses the xxhash implementation by Stephan Brumme. https://create.stephan-brumme.com/xxhash/
 *
 * Inspired by zlib by Jean-loup Gailly and Mark Adler. https://github.com/madler/zlib
 * Also inspired by Zopfli by Lode Vandevenne and Jyrki Alakuijala. https://github.com/google/zopfli
 * With ideas from libdeflate by Eric Biggers. https://github.com/ebiggers/libdeflate
 * Also with ideas from linzip2 by Gary Linscott. https://glinscott.github.io/lz/index.html
 *
 */

#ifndef _FORMAT_H
#define _FORMAT_H

#define MIN_MATCH_SIZE  3
#define MAX_MATCH_SIZE  258
#define MIN_OFFSET 1
#define MAX_OFFSET 32768
#define HISTORY_SIZE 0x8000
#define NCODELENBITS 3
#define NCODELENSYMS 19
#define NLITERALSYMS 288
#define NVALIDLITERALSYMS 286
#define NEODMARKERSYM 256
#define NMATCHLENSYMSTART 257
#define NMATCHLENSYMS 29
#define NOFFSETSYMS 32
#define NVALIDOFFSETSYMS 30

#endif /* _FORMAT_H */
