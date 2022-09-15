#include "zstream.hpp"

#include <iostream>

void usage()
{
  std::cerr << "zpipe usage: zpipe [-d] [--type=(gz|lzma)] < source > dest" << std::endl;
}

bool check_decomp(std::string_view sv_arg)
{
  // do decompression if -d specified
  return sv_arg == "-d";
}

bool check_is_lzma(std::string_view sv_arg)
{
  auto eq_idx = sv_arg.find('=');

  if (eq_idx == sv_arg.npos)
    return false;

  return sv_arg.substr(eq_idx+1) == "xz";
}

template <typename IStr, typename OStr>
void test_stream(IStr& istr, OStr& ostr)
{
  // Copy the entire stream
  std::copy(std::istreambuf_iterator<char>{istr}, std::istreambuf_iterator<char>{}, std::ostreambuf_iterator<char>{ostr});
}

template <typename Tag>
void test_decomp()
{
  using stream_type = zstream::inf_streambuf<Tag, decltype(std::cin)>;
  static_assert(std::is_move_constructible<stream_type>::value);
  static_assert(std::is_move_assignable<stream_type>::value);
  static_assert(std::is_swappable<stream_type>::value);

  // Initialize an inflation buffer
  stream_type isb{std::cin};
  std::istream is{&isb};

  test_stream(is, std::cout);
}

template <typename Tag>
void test_comp()
{
  using stream_type = zstream::def_streambuf<Tag, decltype(std::cout)>;
  static_assert(std::is_move_constructible<stream_type>::value);
  static_assert(std::is_move_assignable<stream_type>::value);
  static_assert(std::is_swappable<stream_type>::value);

  // Initialize a deflation buffer
  stream_type osb{std::cout};
  std::ostream os{&osb};

  test_stream(std::cin, os);
}

/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    std::vector<std::string_view> args{std::next(argv), std::next(argv, argc)};
    auto is_decomp = std::any_of(std::begin(args), std::end(args), check_decomp);
    auto is_gzip = std::any_of(std::begin(args), std::end(args), check_is_lzma);

#ifdef GZIP_INCLUDED
    if (is_gzip) {
      using tag_type = zstream::gzip_tag_t<>;
      if (is_decomp)
        test_decomp<tag_type>();
      else
        test_comp<tag_type>();
    }
#endif

#ifdef LZMA_INCLUDED
    if (!is_gzip) {
      using tag_type = zstream::lzma_tag_t<>;
      if (is_decomp)
        test_decomp<tag_type>();
      else
        test_comp<tag_type>();
    }
#endif

    return 0;
}

