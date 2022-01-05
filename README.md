# zstream
A standard-library-compatible implementation of a stream that compresses or decompresses with zlib.

Compile with

    g++ --std=c++11 zstream.cc -lz

Run with

    ./a.out < foo.txt > foo.gz

These stream buffers can be used with any type that supports `operator>>` and/or `operator<<`.

Example: Compress the first N fibonacci numbers and capture the result as a `std::string`.

    std::ostringstream ostrstr{};
    def_streambuf osb{ostrstr};
    std::ostream os{&osb};

    constexpr std::size_t N = 1 << 8;
    long curr = 0, next = 1;

    for (std::size_t i = 0; i < N; ++i)
    {
        os << curr;
        curr = std::exchange(next, next+curr);
    }
    os << std::flush;
    
    std::string result = ostrstr.str();
