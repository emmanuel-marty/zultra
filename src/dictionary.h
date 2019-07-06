/*
 * dictionary.h - dictionary definitions
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

#ifndef _DICTIONARY_H
#define _DICTIONARY_H

#include "format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef enum _zultra_stream_e zultra_status_t;

/**
 * Load dictionary contents
 *
 * @param pszDictionaryFilename name of dictionary file, or NULL for none
 * @param ppDictionaryData pointer to returned dictionary contents, or NULL for none
 * @param pDictionaryDataSize pointer to returned size of dictionary contents, or 0
 *
 * @return ZULTRA_OK for success, or an error value from zultra_status_t
 */
zultra_status_t zultra_dictionary_load(const char *pszDictionaryFilename, void **ppDictionaryData, int *pDictionaryDataSize);

/**
 * Free dictionary contents
 *
 * @param ppDictionaryData pointer to pointer to dictionary contents
 */
void zultra_dictionary_free(void **ppDictionaryData);

#ifdef __cplusplus
}
#endif

#endif /* _DICTIONARY_H */
