#include "dynamic_erdos_generator.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "ascii_file_stream.h"
#include "binary_file_stream.h"

DynamicErdosGenerator::DynamicErdosGenerator(size_t seed, node_id_t num_vertices, double density,
                                             double portion_delete, double portion_adtl,
                                             size_t rounds)
    : num_vertices(num_vertices),
      seed(seed),
      total_edges(num_vertices * (num_vertices - 1) / 2 * density),
      density(density) {
  // Ensure the density and stream_vary parameters are in bounds
  if (density <= 0 || density > 1) {
    throw StreamException("DynamicErdosGenerator: Density out of range (0, 1]");
  }
  if (portion_delete < 0 || portion_delete > 1) {
    throw StreamException("DynamicErdosGenerator: portion_delete out of range [0, 1]");
  }
  if (portion_adtl < 0 || portion_adtl > 1) {
    throw StreamException("DynamicErdosGenerator: portion_adtl out of range [0, 1]");
  }
  if (rounds == 0 && (portion_adtl > 0 || portion_delete > 0)) {
    throw StreamException("DynamicErdosGenerator: round must be > 0 if adtl or delete > 0");
  }

  // create a random number generator
  std::mt19937_64 gen(seed);

  // Generate set of all possible edges
  std::vector<Edge> all_edges;
  all_edges.reserve(num_vertices * (num_vertices - 1) / 2);
  for (node_id_t i = 0; i < num_vertices; i++)
    for (node_id_t j = i + 1; j < num_vertices; j++) all_edges.push_back({i, j});

  // Permute the set of all possible edges
  std::shuffle(all_edges.begin(), all_edges.end(), gen);

  // begin constructing stream updates vector
  updates.reserve(total_edges + 2 * rounds * total_edges * portion_delete +
                  2 * rounds * (all_edges.size() - total_edges) * portion_adtl);
  true_edges.reserve(total_edges);
  for (edge_id_t i = 0; i < total_edges; i++) {
    updates.push_back({INSERT, all_edges[i]});
    true_edges.push_back(all_edges[i]);
  }
  for (size_t r = 0; r < rounds; r++) {
    // delete and reinsert final
    for (edge_id_t i = 0; i < total_edges * portion_delete; i++) {
      updates.push_back({INSERT, all_edges[i]});
      updates.push_back({INSERT, all_edges[i]});
    }
    // insert and delete not final
    for (edge_id_t i = 0; i < (all_edges.size() - total_edges) * portion_adtl; i++) {
      updates.push_back({INSERT, all_edges[total_edges + i]});
      updates.push_back({INSERT, all_edges[total_edges + i]});
    }
  }

  // set total edges to include extra inserts and deletes
  total_edges = updates.size();

  // shuffle the updates
  std::shuffle(updates.begin(), updates.end(), gen);

  // set the update type appropriately
  // 1. Build adjacency matrix
  std::vector<std::vector<bool>> adj_mat(num_vertices);
  for (node_id_t i = 0; i < num_vertices; i++) {
    adj_mat[i] = std::vector<bool>(num_vertices - i - 1, false);
  }
  // 2. Actually set the types
  for (auto &upd : updates) {
    node_id_t src = std::min(upd.edge.src, upd.edge.dst);
    node_id_t local_dst = std::max(upd.edge.src, upd.edge.dst) - src - 1;
    if (adj_mat[src][local_dst]) {
      upd.type = DELETE;
    } else {
      upd.type = INSERT;
    }
    adj_mat[src][local_dst] = !adj_mat[src][local_dst];
  }
}

void write_to_file(GraphStream *stream, DynamicErdosGenerator &gen) {
  size_t buffer_capacity = 4096;
  GraphUpdate upds[buffer_capacity];
  size_t buffer_size = 0;
  stream->write_header(gen.get_num_vertices(), gen.get_num_edges());

  for (edge_id_t i = 0; i < gen.get_num_edges(); i++) {
    upds[buffer_size++] = gen.get_next_edge();
    if (buffer_size >= buffer_capacity) {
      stream->write_updates(upds, buffer_size);
      buffer_size = 0;
    }
  }
  if (buffer_size > 0) {
    stream->write_updates(upds, buffer_size);
  }
}

void DynamicErdosGenerator::to_binary_file(std::string file_name) {
  edge_idx = 0;
  BinaryFileStream output_stream(file_name, false);
  write_to_file(&output_stream, *this);
}
void DynamicErdosGenerator::to_ascii_file(std::string file_name) {
  edge_idx = 0;
  AsciiFileStream output_stream(file_name, true);
  write_to_file(&output_stream, *this);
}
void DynamicErdosGenerator::write_cumulative_file(std::string file_name) {
  AsciiFileStream output_stream(file_name, false);

  size_t buffer_capacity = 4096;
  GraphUpdate upds[buffer_capacity];
  size_t buffer_size = 0;
  output_stream.write_header(num_vertices, true_edges.size());

  for (auto e : true_edges) {
    upds[buffer_size++] = {INSERT, e};
    if (buffer_size >= buffer_capacity) {
      output_stream.write_updates(upds, buffer_size);
      buffer_size = 0;
    }
  }
  if (buffer_size > 0) {
    output_stream.write_updates(upds, buffer_size);
  }
}

GraphUpdate DynamicErdosGenerator::get_next_edge() { return updates[edge_idx++]; }
