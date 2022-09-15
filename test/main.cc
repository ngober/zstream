#include <iostream>
#include <fstream>
#include "../zstream.hpp"

template <typename Tag>
bool do_inf_test(const std::string& infile_name, const std::string& outfile_name)
{
  // Open files
  std::ifstream infile{infile_name};
  std::ifstream outfile{outfile_name};

  // Initialize a inflation/deflation buffer
  zstream::inf_streambuf comp_buffer{Tag{}, infile};
  std::istream comp_stream{&comp_buffer};

  static_assert(std::is_move_constructible<decltype(comp_buffer)>::value);
  static_assert(std::is_move_assignable<decltype(comp_buffer)>::value);
  static_assert(std::is_swappable<decltype(comp_buffer)>::value);

  return std::equal(std::istreambuf_iterator<char>{comp_stream}, std::istreambuf_iterator<char>{}, std::istreambuf_iterator<char>{outfile});
}

template <typename Tag>
bool do_bidirectional_test(const std::vector<std::string>& decompressed_names, const std::vector<std::string>& compressed_names)
{
  std::vector<bool> comp_results, decomp_results;
  //std::transform(std::cbegin(decompressed_names), std::cend(decompressed_names), std::cbegin(compressed_names), std::back_inserter(comp_results), do_test<def_stream_type>);
  std::transform(std::cbegin(compressed_names), std::cend(compressed_names), std::cbegin(decompressed_names), std::back_inserter(decomp_results), do_inf_test<Tag>);

  return
    std::all_of(std::cbegin(decomp_results), std::cend(decomp_results), [](auto x){ return x; });
    //&& std::all_of(std::cbegin(comp_results), std::cend(comp_results), [](auto x){ return x; });
}

std::vector<std::string> decomp_fnames{{
  "data/1.txt"
}};

int main(int argc, char** argv)
{
  std::vector<std::string> gzip_comp_fnames;
  std::vector<std::string> lzma_comp_fnames;

  std::transform(std::cbegin(decomp_fnames), std::cend(decomp_fnames), std::back_inserter(gzip_comp_fnames), [](auto x){ return x + ".gz"; });
  std::transform(std::cbegin(decomp_fnames), std::cend(decomp_fnames), std::back_inserter(lzma_comp_fnames), [](auto x){ return x + ".xz"; });

  bool result = do_bidirectional_test<zstream::gzip_tag_t<>>(decomp_fnames, gzip_comp_fnames) && do_bidirectional_test<zstream::lzma_tag_t<>>(decomp_fnames, lzma_comp_fnames);
  if (!result)
    std::cout << "fail" << std::endl;
  return result ? 0 : 1;
}

