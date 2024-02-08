#pragma once
#include <graph_zeppelin_common.h>
#include "permuted_set.h"
#include "graph_stream.h"

class StaticErdosGenerator {
 private:
  node_id_t num_vertices;
  double density;
  size_t seed;
  edge_id_t total_edges;
  PermutedSet permute;

  edge_id_t edge_idx = 0;
  edge_id_t skip = 0;
  size_t v_bits;
 public:
  StaticErdosGenerator(node_id_t num_vertices, double density, size_t seed);

  // these functions write all the stream edges to a file
  void to_binary_file(std::string file_name);
  void to_ascii_file(std::string file_name);

  GraphStreamUpdate get_next_edge();

  // getters
  node_id_t get_num_vertices() { return num_vertices; }
  edge_id_t get_num_edges() { return total_edges; }
};
