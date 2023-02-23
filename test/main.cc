#include "../zstream.hpp"

#include <iostream>
#include <fstream>
#include <limits>
#include <sstream>
#include <typeinfo>

template <typename Tag>
bool do_inf_test(const std::string& infile_name, const std::string& outfile_name)
{
  std::cout << "inflate<" << typeid(Tag).name() << ">: " << infile_name << " -> " << outfile_name << std::endl;

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
bool do_def_test(const std::string& infile_name, const std::string& outfile_name)
{
  std::cout << "deflate<" << typeid(Tag).name() << ">: " << infile_name << " -> " << outfile_name << std::endl;

  // Open files
  std::ifstream infile{infile_name};
  std::ifstream outfile{outfile_name};

  // Initialize a inflation/deflation buffer
  std::stringstream result_stream;
  zstream::def_streambuf decomp_buffer{Tag{}, result_stream};
  std::ostream decomp_stream{&decomp_buffer};

  std::copy(std::istreambuf_iterator<char>{infile}, std::istreambuf_iterator<char>{}, std::ostreambuf_iterator<char>{decomp_stream});
  decomp_stream.flush();

  static_assert(std::is_move_constructible<decltype(decomp_buffer)>::value);
  static_assert(std::is_move_assignable<decltype(decomp_buffer)>::value);
  static_assert(std::is_swappable<decltype(decomp_buffer)>::value);

  //DEBUG
  std::ofstream debug_file{outfile_name + ".dbg"};
  debug_file << result_stream.str() << std::endl;
  //END DEBUG

  return std::equal(std::istreambuf_iterator<char>{result_stream}, std::istreambuf_iterator<char>{}, std::istreambuf_iterator<char>{outfile});
}

template <typename Tag>
bool do_bidirectional_test(const std::vector<std::string>& decompressed_names, const std::vector<std::string>& compressed_names)
{
  std::vector<std::pair<std::string, std::string>> decomp_comp_pairs;
  std::transform(std::cbegin(decompressed_names), std::cend(decompressed_names), std::cbegin(compressed_names), std::back_inserter(decomp_comp_pairs), [](auto x, auto y){ return std::make_pair(x,y); });

  return
    std::all_of(std::cbegin(decomp_comp_pairs), std::cend(decomp_comp_pairs), [](auto x){ return do_inf_test<Tag>(x.second, x.first); }) &&
    std::all_of(std::cbegin(decomp_comp_pairs), std::cend(decomp_comp_pairs), [](auto x){ return do_def_test<Tag>(x.first, x.second); });
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

  bool result = do_bidirectional_test<zstream::lzma_tag_t<>>(decomp_fnames, lzma_comp_fnames) && do_bidirectional_test<zstream::gzip_tag_t<>>(decomp_fnames, gzip_comp_fnames);
  if (!result)
    std::cout << "fail" << std::endl;
  return result ? 0 : 1;
}

