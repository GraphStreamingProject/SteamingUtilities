#include <iostream>
#include <cmath>
#include <string>

#include "single_cut_generator.h"

int main() {
  std::cout << "SINGLE CUT STREAM" << std::endl;
  SingleCutGenerator scut_stream(8192);
  std::cout << "num_vertices = " << scut_stream.get_num_vertices() << std::endl;
  std::cout << "num_edges    = " << scut_stream.get_num_edges() << std::endl;

  // write out to binary stream file
  std::string file_name = "scut_" + std::to_string(int(log2(scut_stream.get_num_vertices()))) + "_stream_binary";
  scut_stream.to_binary_file(file_name);
}
