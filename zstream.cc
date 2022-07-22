#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <zlib.h>

#include <cstring>

#define Z_STREAM_INIT_ARGS Z_NULL, 0, 0, Z_NULL, 0, 0, NULL, NULL, Z_NULL, Z_NULL, Z_NULL

template< typename OStrm >
class def_streambuf : public std::basic_streambuf<typename OStrm::char_type, std::char_traits<typename OStrm::char_type>>
{
    private:
        using base_type = std::basic_streambuf<typename OStrm::char_type, std::char_traits<typename OStrm::char_type>>;
        using int_type  = typename base_type::int_type;
        using char_type = typename base_type::char_type;
        using strm_in_buf_type = std::remove_pointer<decltype(z_stream::next_in)>::type;
        using strm_out_buf_type = std::remove_pointer<decltype(z_stream::next_out)>::type;

        constexpr static std::size_t CHUNK = (1 << 16);

        std::array<char_type, CHUNK> in_buf;
        std::unique_ptr<z_stream, decltype(&deflateEnd)> strm{new z_stream{Z_STREAM_INIT_ARGS}, &deflateEnd};
        typename std::add_pointer<OStrm>::type dest;

        bool def(int flush);

    public:
        explicit def_streambuf(OStrm &out, int level = Z_DEFAULT_COMPRESSION) : dest(&out)
        {
            deflateInit(strm.get(), level);
            this->setp(in_buf.data(), std::next(in_buf.data(), in_buf.size() - 1));
        }

    protected:
        int_type overflow(int_type ch) override;
        int sync() override;
};

template < typename O >
int def_streambuf<O>::sync()
{
    return def(Z_FINISH) ? 0 : -1;
}

template < typename O >
auto def_streambuf<O>::overflow(int_type ch) -> int_type
{
    if (ch == base_type::traits_type::eof())
        return ch;

    *this->pptr() = ch;
    this->pbump(1);

    auto result = def(Z_NO_FLUSH);
    return result ? ch : base_type::traits_type::eof();
}

template < typename O >
bool def_streambuf<O>::def(int flush)
{
    std::array<strm_in_buf_type, std::tuple_size<decltype(this->in_buf)>::value> uns_in_buf;
    std::array<strm_out_buf_type, CHUNK> uns_out_buf;

    std::memcpy(uns_in_buf.data(), this->pbase(), std::distance(this->pbase(), this->pptr()));

    strm->avail_in = std::distance(this->pbase(), this->pptr());
    strm->next_in  = uns_in_buf.data();
    int ret;

    do
    {
        strm->avail_out = uns_out_buf.size();
        strm->next_out  = uns_out_buf.data();
        ret = deflate(strm.get(), flush);

        // Write to file
        std::array<char_type, std::size(uns_out_buf)> out_buf;
        std::memcpy(out_buf.data(), uns_out_buf.data(), uns_out_buf.size() - strm->avail_out);
        dest->write(out_buf.data(), uns_out_buf.size() - strm->avail_out);
    } while (ret == Z_OK && strm->avail_out == 0);

    auto new_input_begin = std::distance(this->pbase(), this->pptr()) - strm->avail_in;
    std::copy_n(std::next(std::begin(this->in_buf), new_input_begin), strm->avail_in, std::begin(this->in_buf));

    this->pbump(-1*new_input_begin); // reset pptr
    return ret == Z_OK || ret == Z_STREAM_END;
}

template< typename IStrm >
class inf_streambuf : public std::basic_streambuf<typename IStrm::char_type, std::char_traits<typename IStrm::char_type>>
{
    private:
        using base_type = std::basic_streambuf<typename IStrm::char_type, std::char_traits<typename IStrm::char_type>>;
        using int_type  = typename base_type::int_type;
        using char_type = typename base_type::char_type;
        using strm_in_buf_type = std::remove_pointer<decltype(z_stream::next_in)>::type;
        using strm_out_buf_type = std::remove_pointer<decltype(z_stream::next_out)>::type;

        constexpr static std::size_t CHUNK = (1 << 16);

        std::array<strm_in_buf_type, CHUNK> in_buf;
        std::array<char_type, CHUNK> out_buf;
        std::unique_ptr<z_stream, decltype(&inflateEnd)> strm{new z_stream{Z_STREAM_INIT_ARGS}, &inflateEnd};
        typename std::add_pointer<IStrm>::type src;

    public:
        explicit inf_streambuf(IStrm &in, int window_bits = 15+32) : src(&in)
        {
            inflateInit2(strm.get(), window_bits);
        }

        std::size_t bytes_read() const
        {
            return strm->total_out - (this->egptr() - this->gptr());
        }

    protected:
        int_type underflow() override;
};

template < typename I >
auto inf_streambuf<I>::underflow() -> int_type
{
    std::array<strm_out_buf_type, std::tuple_size<decltype(out_buf)>::value> uns_out_buf;

    strm->avail_out = uns_out_buf.size();
    strm->next_out = uns_out_buf.data();
    do
    {
        // Check to see if we have consumed all available input
        if (strm->avail_in == 0)
        {
            // Check to see if the input stream is sane
            if (src->fail())
            {
                this->setg(this->out_buf.data(), this->out_buf.data(), this->out_buf.data());
                return base_type::underflow();
            }

            // Read data from the stream and convert to zlib-appropriate format
            std::array<char_type, std::tuple_size<decltype(in_buf)>::value> sig_in_buf;
            src->read(sig_in_buf.data(), sig_in_buf.size());
            std::memcpy(in_buf.data(), sig_in_buf.data(), src->gcount());

            // Record that bytes are available in in_buf
            strm->avail_in = src->gcount();
            strm->next_in = in_buf.data();

            // If we failed to get any data
            if (strm->avail_in == 0)
            {
                this->setg(this->out_buf.data(), this->out_buf.data(), this->out_buf.data());
                return base_type::underflow();
            }
        }

        // Perform inflation
        inflate(strm.get(), Z_NO_FLUSH);
    }
    // Repeat until we actually get new output
    while (strm->avail_out == uns_out_buf.size());

    // Copy into a format appropriate for the stream
    std::memcpy(this->out_buf.data(), uns_out_buf.data(), uns_out_buf.size() - strm->avail_out);

    this->setg(this->out_buf.data(), this->out_buf.data(), std::next(this->out_buf.data(), uns_out_buf.size() - strm->avail_out));
    return base_type::traits_type::to_int_type(this->out_buf.front());
}

/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    // Tests
    static_assert(std::is_move_constructible<inf_streambuf<decltype(std::cin)>>::value);
    static_assert(std::is_move_assignable<inf_streambuf<decltype(std::cin)>>::value);
    static_assert(std::is_swappable<inf_streambuf<decltype(std::cin)>>::value);
    static_assert(std::is_move_constructible<def_streambuf<decltype(std::cout)>>::value);
    static_assert(std::is_move_assignable<def_streambuf<decltype(std::cout)>>::value);
    static_assert(std::is_swappable<def_streambuf<decltype(std::cout)>>::value);

    // Report usage
    if (argc > 2 || (argc == 2 && argv[1] != std::string{"-d"}))
    {
        std::cerr << "zpipe usage: zpipe [-d] < source > dest" << std::endl;
        return 1;
    }

    // do decompression if -d specified
    bool is_decomp = (argc > 1);

    // Initialize a deflation buffer
    def_streambuf osb{std::cout};
    std::ostream os{&osb};

    // Initialize an inflation buffer
    inf_streambuf isb{std::cin};
    std::istream is{&isb};

    // Decide which direction to operate
    std::ostream *out = is_decomp ? &std::cout : &os;
    std::istream *in  = is_decomp ? &is        : &std::cin;

    // Copy the entire stream
    std::copy(std::istreambuf_iterator<char>{*in}, std::istreambuf_iterator<char>{}, std::ostreambuf_iterator<char>{*out});

    return 0;
}

