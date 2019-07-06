zultra -- fast near-optimal deflate/zlib/gzip compression
=========================================================

zultra is a command-line tool and a compression-only library that produces compressed bitstreams in the zlib (RFC 1950), deflate (RFC 1951), and gzip (RFC 1952) formats, fully compatible with exisiting tools and libraries using these formats, such as zip and png.

The zultra library creates near-optimal compressed bitstreams with a ratio similar to zopfli, at approximately 50% of the speed of zlib compression level 9. 


License:

* The zultra code is available under the Zlib license.
* The match finder (matchfinder.c) is available under the CC0 license due to using portions of code from Eric Bigger's Wimlib in the suffix array-based matchfinder.
* huffutils.c is available under the Apache License, Version 2.0.
