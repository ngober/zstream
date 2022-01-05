# zstream
A standard-library-compatible implementation of a stream that compresses or decompresses with zlib.

Compile with

    g++ --std=c++11 zstream.cc -lz

These stream buffers can be used with any type that supports `operator>>` and/or `operator<<`.

Example: Compress and print the first N fibonacci numbers

    def_streambuf osb{std::cout, Z_DEFAULT_COMPRESSION};
    std::ostream os{&osb};

    constexpr std::size_t N = 1 << 16;
    long curr = 0, next = 1;

    for (std::size_t i = 0; i < N; ++i)
    {
        os << curr;
        curr = std::exchange(next, next+curr);
    }
    os << std::flush;



