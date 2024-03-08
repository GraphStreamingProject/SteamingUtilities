
#include <iostream>

#include "dynamic_erdos_generator.h"
#include "static_erdos_generator.h"

std::string type_string(uint8_t type) {
  if (type == INSERT)
    return "INSERT";
  else if (type == DELETE)
    return "DELETE";
  else
    return "BREAKPOINT";
}

// just a quick little tool to visually test that the streams created by our erdos generators
// look reasonable
int main() {
  std::cout << "STATIC STREAM" << std::endl;
  StaticErdosGenerator st_stream(287424, 8, 0.5);
  std::cout << "num_vertices = " << st_stream.get_num_vertices() << std::endl;
  std::cout << "num_edges    = " << st_stream.get_num_edges() << std::endl;

  for (edge_id_t e = 0; e < st_stream.get_num_edges(); e++) {
    GraphStreamUpdate upd = st_stream.get_next_edge();
    std::cout << type_string(upd.type) << " " << upd.edge.src << " " << upd.edge.dst << std::endl;
  }

  std::cout << "DYNAMIC STREAM" << std::endl;
  DynamicErdosGenerator dy_stream(287424, 1024, 0.002, 0.5, 0.1, 3);
  std::cout << "num_vertices = " << dy_stream.get_num_vertices() << std::endl;
  std::cout << "num_edges    = " << dy_stream.get_num_edges() << std::endl;

  // for (edge_id_t e = 0; e < dy_stream.get_num_edges(); e++) {
  //   GraphStreamUpdate upd = dy_stream.get_next_edge();
  //   std::cout << type_string(upd.type) << " " << upd.edge.src << " " << upd.edge.dst << std::endl;
  // }

  // write out stream and cumulative to different file formats
  dy_stream.to_binary_file("dy_erdos_stream_binary.data");
  dy_stream.to_ascii_file("dy_erdos_stream_ascii.txt");
  dy_stream.write_cumulative_file("dy_erdos_cumul.txt");
}
