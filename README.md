zultra -- fast near-optimal deflate/zlib/gzip compression
=========================================================

zultra is a command-line tool and a compression-only library that produces compressed bitstreams in the zlib (RFC 1950), deflate (RFC 1951), and gzip (RFC 1952) formats, fully compatible with exisiting tools and libraries using these formats, such as zip and png.

The zultra library creates near-optimal compressed bitstreams with a ratio similar to zopfli (gaining around 4% on zlib), at approximately 25-50% of the speed of zlib compression level 9, on average. 

zultra is written in plain C. The maximum block size (used to optimize the output) can be tuned, for instance for on-device compression scenarios. The compressor fully supports streaming with an API very similar to zlib. 

Example benchmarks (using lzbench 1.7.3):

    enwik8 (100000000)

    Compressor name         Compress. Decompress. Compr. size  Ratio Filename
    memcpy                   8846 MB/s  9006 MB/s   100000000 100.00 enwik8
    zultra 1.0.0             3.38 MB/s   269 MB/s    35029585  35.03 enwik8
    zlib 1.2.11 -9             16 MB/s   253 MB/s    36475792  36.48 enwik8
    libdeflate 1.0 -12       6.55 MB/s   581 MB/s    35100568  35.10 enwik8
    zopfli 1.0.0             0.38 MB/s   264 MB/s    34966066  34.97 enwik8

    mozilla, silesia corpus (51220480)

    Compressor name         Compress. Decompress. Compr. size  Ratio Filename
    memcpy                   9488 MB/s  9465 MB/s    51220480 100.00 mozilla
    zultra 1.0.0             3.48 MB/s   277 MB/s    18280189  35.69 mozilla
    zlib 1.2.11 -9           6.70 MB/s   279 MB/s    19044396  37.18 mozilla
    libdeflate 1.0 -12       7.65 MB/s   586 MB/s    18308548  35.74 mozilla
    zopfli 1.0.0             0.28 MB/s   279 MB/s    18317347  35.76 mozilla

    pariah.utx (24375895)

    Compressor name         Compress. Decompress. Compr. size  Ratio Filename
    memcpy                   9537 MB/s  9629 MB/s    24375895 100.00 pariah.utx
    zultra 1.0.0             3.75 MB/s   302 MB/s     7892356  32.38 pariah.utx
    zlib 1.2.11 -9           3.84 MB/s   301 MB/s     8214524  33.70 pariah.utx
    libdeflate 1.0 -12       7.05 MB/s   675 MB/s     7914073  32.47 pariah.utx
    zopfli 1.0.0             0.16 MB/s   302 MB/s     7886453  32.35 pariah.utx

    bootstrap.min.js (48944)

    Compressor name         Compress. Decompress. Compr. size  Ratio Filename
    memcpy                  38813 MB/s 41025 MB/s       48944 100.00 bootstrap.min.js
    zultra 1.0.0             3.15 MB/s   353 MB/s       12599  25.74 bootstrap.min.js
    zlib 1.2.11 -9             32 MB/s   355 MB/s       13034  26.63 bootstrap.min.js
    libdeflate 1.0 -12       5.92 MB/s   881 MB/s       12617  25.78 bootstrap.min.js
    zopfli 1.0.0             0.42 MB/s   345 MB/s       12599  25.74 bootstrap.min.js

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
