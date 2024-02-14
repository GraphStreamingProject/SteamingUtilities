#pragma once
#include "stream_types.h"
#include <cmath>
#include <string>
#include <vector>

// Single cut stream generator
// Builds two connected components then repeatedly updates edges in the cut between the two
// Several rounds of adding n edges and then removing n edges, and adding and removing a single edge n times
class SingleCutGenerator {
 private:
  std::vector<GraphStreamUpdate> updates;
  node_id_t num_vertices;
  edge_id_t num_edges;
  edge_id_t edge_idx = 0;
 public:
  /*
   * Constructor
   * @param num_vertices    number of vertices in the graph
   * @param rounds          Number of rounds
   */
  SingleCutGenerator(node_id_t num_vertices, size_t rounds = 0);

  // these functions write all the stream edges to a file
  void to_binary_file(std::string file_name);
  void to_ascii_file(std::string file_name);
  void write_cumulative_file(std::string file_name);

  GraphStreamUpdate get_next_edge();

  // getters
  node_id_t get_num_vertices() { return num_vertices; }
  edge_id_t get_num_edges() { return num_edges; }
};