# zstream
A standard-library-compatible implementation of a stream that compresses or decompresses with zlib.

Compile with

    g++ --std=c++11 zstream.cc -lz


Run with

    ./a.out < foo.txt > foo.gz
