/*
 * MIT License
 *
 * Copyright (c) 2021 Nathan Gober
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <streambuf>
#include <type_traits>
#include <utility>
#include <vector>

#if __has_include(<zlib.h>)

#include <zlib.h>
#define ZLIB_INCLUDED

#endif

#if __has_include(<lzma.h>)

#include <lzma.h>
#define LZMA_INCLUDED

#endif

namespace zstream {

enum class status_t
{
  CAN_CONTINUE,
  END,
  ERROR
};

#ifdef ZLIB_INCLUDED
template <int window = 15+16, int compression = Z_DEFAULT_COMPRESSION>
struct gzip_tag_t
{
  using state_type = z_stream;
  using in_char_type = std::remove_pointer_t<decltype(state_type::next_in)>;
  using out_char_type = std::remove_pointer_t<decltype(state_type::next_out)>;
  using deflate_state_type = std::unique_ptr<state_type, decltype(&::deflateEnd)>;
  using inflate_state_type = std::unique_ptr<state_type, decltype(&::inflateEnd)>;
  using status_type = status_t;

  static status_type deflate(deflate_state_type &x, bool flush) {
    auto ret = ::deflate(x.get(), flush ? Z_FINISH : Z_NO_FLUSH);
    if (ret == Z_OK)
      return status_type::CAN_CONTINUE;
    if (ret == Z_STREAM_END)
      return status_type::END;
    return status_type::ERROR;
  }

  static status_type inflate(inflate_state_type &x) {
    ::inflate(x.get(), Z_BLOCK);
    return status_type::CAN_CONTINUE;
  }

  static deflate_state_type new_deflate_state() {
    deflate_state_type state{new state_type{Z_NULL, 0, 0, Z_NULL, 0, 0, NULL, NULL, Z_NULL, Z_NULL, Z_NULL}, &::deflateEnd};
    ::deflateInit(state.get(), compression);
    return state;
  }

  static inflate_state_type new_inflate_state() {
    inflate_state_type state{new state_type{Z_NULL, 0, 0, Z_NULL, 0, 0, NULL, NULL, Z_NULL, Z_NULL, Z_NULL}, &::inflateEnd};
    ::inflateInit2(state.get(), window);
    return state;
  }
};
#endif

#ifdef LZMA_INCLUDED
template <uint32_t flags = 0>
struct lzma_tag_t
{
  using state_type = lzma_stream;
  using in_char_type = std::remove_const_t<std::remove_pointer_t<decltype(state_type::next_in)>>;
  using out_char_type = std::remove_pointer_t<decltype(state_type::next_out)>;
  using deflate_state_type = std::unique_ptr<state_type, decltype(&::lzma_end)>;
  using inflate_state_type = std::unique_ptr<state_type, decltype(&::lzma_end)>;
  using status_type = status_t;

  static status_type deflate(deflate_state_type &x, bool flush) {
    auto ret = ::lzma_code(x.get(), flush ? LZMA_FULL_FLUSH : LZMA_RUN);
    if (ret == LZMA_OK)
      return status_type::CAN_CONTINUE;
    else if (ret == LZMA_STREAM_END)
      return status_type::END;
    else
      return status_type::ERROR;
  }

  static status_type inflate(inflate_state_type &x) {
    auto ret = ::lzma_code(x.get(), LZMA_RUN);
    if (ret == LZMA_OK)
      return status_type::CAN_CONTINUE;
    else if (ret == LZMA_STREAM_END)
      return status_type::END;
    else
      return status_type::ERROR;
  }

  static deflate_state_type new_deflate_state() {
    deflate_state_type state{new state_type, &::lzma_end};
    *state = LZMA_STREAM_INIT;
    auto ret = ::lzma_easy_encoder(state.get(), LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
    assert(ret == LZMA_OK);
    return state;
  }

  static inflate_state_type new_inflate_state() {
    inflate_state_type state{new state_type, &::lzma_end};
    *state = LZMA_STREAM_INIT;
    auto ret = ::lzma_stream_decoder(state.get(), std::numeric_limits<uint64_t>::max(), flags);
    assert(ret == LZMA_OK);
    return state;
  }
};
#endif

template< typename Tag, typename OStrm >
class def_streambuf : public std::basic_streambuf<typename OStrm::char_type, std::char_traits<typename OStrm::char_type>>
{
    private:
        using base_type = std::basic_streambuf<typename OStrm::char_type, std::char_traits<typename OStrm::char_type>>;
        using int_type  = typename base_type::int_type;
        using char_type = typename base_type::char_type;
        using strm_in_buf_type = typename Tag::in_char_type;
        using strm_out_buf_type = typename Tag::out_char_type;

        constexpr static std::size_t CHUNK = (1 << 16);

        std::array<char_type, CHUNK> in_buf;
        typename Tag::deflate_state_type strm = Tag::new_deflate_state();
        typename std::add_pointer<OStrm>::type dest;

        bool def(bool flush);

    public:
        explicit def_streambuf(OStrm &out) : dest(&out)
        {
            this->setp(in_buf.data(), std::next(in_buf.data(), in_buf.size() - 1));
        }

        explicit def_streambuf(Tag, OStrm &out) : def_streambuf(out) {}

    protected:
        int_type overflow(int_type ch) override;
        int sync() override;
};

template < typename T, typename O >
int def_streambuf<T,O>::sync()
{
    return def(true) ? 0 : -1;
}

template < typename T, typename O >
auto def_streambuf<T,O>::overflow(int_type ch) -> int_type
{
    if (ch == base_type::traits_type::eof())
        return ch;

    *this->pptr() = ch;
    this->pbump(1);

    auto result = def(false);
    return result ? ch : base_type::traits_type::eof();
}

template < typename T, typename O >
bool def_streambuf<T,O>::def(bool flush)
{
    std::array<strm_in_buf_type, std::tuple_size<decltype(this->in_buf)>::value> uns_in_buf;
    std::array<strm_out_buf_type, CHUNK> uns_out_buf;

    std::memcpy(uns_in_buf.data(), this->pbase(), std::distance(this->pbase(), this->pptr()));

    strm->avail_in = std::distance(this->pbase(), this->pptr());
    strm->next_in  = uns_in_buf.data();
    typename T::status_type ret;

    do
    {
        strm->avail_out = uns_out_buf.size();
        strm->next_out  = uns_out_buf.data();
        ret = T::deflate(strm, flush);

        // Write to file
        std::array<char_type, std::size(uns_out_buf)> out_buf;
        std::memcpy(out_buf.data(), uns_out_buf.data(), uns_out_buf.size() - strm->avail_out);
        dest->write(out_buf.data(), uns_out_buf.size() - strm->avail_out);
    } while (ret == T::status_type::CAN_CONTINUE && strm->avail_out == 0);

    auto new_input_begin = std::distance(this->pbase(), this->pptr()) - strm->avail_in;
    std::copy_n(std::next(std::begin(this->in_buf), new_input_begin), strm->avail_in, std::begin(this->in_buf));

    this->pbump(-1*new_input_begin); // reset pptr
    return ret == T::status_type::CAN_CONTINUE || ret == T::status_type::END;
}

template< typename Tag, typename IStrm >
class inf_streambuf : public std::basic_streambuf<typename IStrm::char_type, std::char_traits<typename IStrm::char_type>>
{
    private:
        using base_type = std::basic_streambuf<typename IStrm::char_type, std::char_traits<typename IStrm::char_type>>;
        using int_type  = typename base_type::int_type;
        using char_type = typename base_type::char_type;
        using strm_in_buf_type = typename Tag::in_char_type;
        using strm_out_buf_type = typename Tag::out_char_type;

        constexpr static std::size_t CHUNK = (1 << 16);

        std::array<strm_in_buf_type, CHUNK> in_buf;
        std::array<char_type, CHUNK> out_buf;
        typename Tag::inflate_state_type strm = Tag::new_inflate_state();
        typename std::add_pointer<IStrm>::type src;

    public:
        explicit inf_streambuf(IStrm &in) : src(&in) {}
        explicit inf_streambuf(Tag, IStrm &in) : inf_streambuf(in) {}

        std::size_t bytes_read() const
        {
            return strm->total_out - (this->egptr() - this->gptr());
        }

    protected:
        int_type underflow() override;
};

template < typename T, typename I >
auto inf_streambuf<T,I>::underflow() -> int_type
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
        auto result = T::inflate(strm);
        assert(result == T::status_type::CAN_CONTINUE || result == T::status_type::END);
    }
    // Repeat until we actually get new output
    while (strm->avail_out == uns_out_buf.size());

    // Copy into a format appropriate for the stream
    std::memcpy(this->out_buf.data(), uns_out_buf.data(), uns_out_buf.size() - strm->avail_out);

    this->setg(this->out_buf.data(), this->out_buf.data(), std::next(this->out_buf.data(), uns_out_buf.size() - strm->avail_out));
    return base_type::traits_type::to_int_type(this->out_buf.front());
}

} // namespace zstream

