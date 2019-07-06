zultra -- fast near-optimal deflate/zlib/gzip compression
=========================================================

zultra is a command-line tool and a compression-only library that produces compressed bitstreams in the zlib (RFC 1950), deflate (RFC 1951), and gzip (RFC 1952) formats, fully compatible with exisiting tools and libraries using these formats, such as zip and png.

The zultra library creates near-optimal compressed bitstreams with a ratio similar to zopfli (gaining around 4% on zlib), at approximately 25-50% of the speed of zlib compression level 9, on average. 

zultra is meant to replace zlib compression in scenarios where zopfli can't be used:
* too computationally expensive. zultra compresses to within 0,1% of zopfli, 10 times faster, on average.
* a streaming API is needed. zultra provides an API similar to zlib's deflate.

zultra is written in plain C. The maximum block size (used to optimize the output) can be tuned, for instance for on-device compression scenarios.

Example benchmarks:

    enwik8 (100000000)

                    compr.time   compr.size
    gzip 1.8 -9     7.78         36445252
    zultra 1.0.0    27.86        35029597
    zopfli 1.0.2    294.38       34966078

    mozilla, silesia corpus (51220480)

    gzip 1.8 -9     7.98         18994137
    zultra 1.0.0    14.74        18280201
    zopfli 1.0.2    210.03       18317359

    pariah.utx (24375895)

                    compr.time   compr.size
    gzip 1.8        8.42         8238060
    zultra 1.0.0    6.14         7892368
    zopfli 1.0.2    197.03       7886465

    bootstrap.min.js (48944)

                    compr.time   compr.size
    gzip 1.8        0.03         13023
    zultra 1.0.0    0.02         12611
    zopfli 1.0.2    0.17         12611

Inspirations:

* The original library itself, [zlib](https://github.com/madler/zlib) by Jean-loup Gailly and Mark Adler.
* Some huffman RLE encoding optimizations tricks in [Zopfli](https://github.com/google/zopfli) by Lode Vandevenne and Jyrki Alakuijala.
* The suffix array intervals in [Wimlib](https://wimlib.net/git/?p=wimlib;a=tree) by Eric Biggers.
* The block splitting heuristics in [libdeflate](https://github.com/ebiggers/libdeflate), also by Eric Biggers.
* Some general LZ-huffman ideas in [linzip2](https://glinscott.github.io/lz/index.html) by Gary Linscott.
* Fast huffman codelength building from [this research paper](http://hjemmesider.diku.dk/~jyrki/Paper/WADS95.pdf) by Moffat and Katajainen.

License:

* The zultra code is available under the Zlib license.
* The match finder (matchfinder.c) is available under the CC0 license due to using portions of code from Eric Bigger's Wimlib in the suffix array-based matchfinder.
* huffutils.c is available under the Apache License, Version 2.0.
