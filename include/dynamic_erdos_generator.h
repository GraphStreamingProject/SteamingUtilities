#pragma once
#include "stream_types.h"
#include <vector>
#include <string>

// Fairly inefficient dynamic erdos graph stream generator
// Builds a vector of resulting edges, augments it with insertions and deletions, then shuffles
// all the edge updates and labels them appropriately.
class DynamicErdosGenerator {
 private:
  std::vector<GraphUpdate> updates;
  std::vector<Edge> true_edges;
  node_id_t num_vertices;
  size_t seed;
  edge_id_t total_edges;
  double density;
  double stream_vary;
  edge_id_t edge_idx = 0;
 public:
  /*
   * Constructor
   * @param seed            the seed to our vector shuffles
   * @param num_vertices    number of vertices in the graph
   * @param density         resulting density of the graph after the stream
   * @param portion_delete  proportion of edges to delete from stream and reinsert
   * @param portion_adtl    proportion of edges not in graph to add and then delete
   * @param rounds          Number of times to perform the portion inserts/deletes
   */
  DynamicErdosGenerator(size_t seed, node_id_t num_vertices, double density, double portion_delete,
                        double portion_adtl, size_t rounds);

  // these functions write all the stream edges to a file
  void to_binary_file(std::string file_name);
  void to_ascii_file(std::string file_name);
  void write_cumulative_file(std::string file_name);

  GraphUpdate get_next_edge();

  // getters
  node_id_t get_num_vertices() { return num_vertices; }
  edge_id_t get_num_edges() { return total_edges; }
};
