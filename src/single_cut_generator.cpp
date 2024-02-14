#include "single_cut_generator.h"
#include "ascii_file_stream.h"
#include "binary_file_stream.h"

SingleCutGenerator::SingleCutGenerator(node_id_t num_vertices, size_t rounds)
    : num_vertices(num_vertices) {
    if (log2(num_vertices) - size_t(log2(num_vertices)) != 0) {
        throw StreamException("SingleCutGenerator: Number of vertices must be a power of 2!");
    }
    if (rounds == 0) // Default number of rounds is n/8
        rounds = num_vertices / 8;
    num_edges = num_vertices-2 + 2*rounds*num_vertices;

    updates.reserve(num_edges);
    // Build two large components
    GraphStreamUpdate update;
    update.type = INSERT;
    for (node_id_t u=0; u<num_vertices/2-1; u++) {
        update.edge = {u,u+1};
        updates.push_back(update);
        update.edge = {u+num_vertices/2,u+1+num_vertices/2};
        updates.push_back(update);
    }
    // Repeatedly add and remove edges between the two components rounds times
    for (int i = 0; i < rounds; i++) {
        std::cout << "GENERATING ROUND " << i << " OF " << rounds << std::endl;
        // First insert a bunch of edges across the cut and then delete them
        update.type = INSERT;
        for (node_id_t u=0; u<num_vertices/2; u++) {
            update.edge = {u,u+num_vertices/2};
            updates.push_back(update);
        }
        update.type = DELETE;
        for (node_id_t u=0; u<num_vertices/2; u++) {
            update.edge = {u,u+num_vertices/2};
            updates.push_back(update);
        }
        // Next repeatedly insert and delete one edge across the cut
        for (node_id_t u=0; u<num_vertices/2; u++) {
            update.type = INSERT;
            update.edge = {u,u+num_vertices/2};
            updates.push_back(update);
            update.type = DELETE;
            update.edge = {u,u+num_vertices/2};
            updates.push_back(update);
        }
    }
}

void write_to_file(GraphStream *stream, SingleCutGenerator &gen) {
  size_t buffer_capacity = 4096;
  GraphStreamUpdate upds[buffer_capacity];
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

void SingleCutGenerator::to_binary_file(std::string file_name) {
  edge_idx = 0;
  BinaryFileStream output_stream(file_name, false);
  write_to_file(&output_stream, *this);
}
void SingleCutGenerator::to_ascii_file(std::string file_name) {
  edge_idx = 0;
  AsciiFileStream output_stream(file_name, true);
  write_to_file(&output_stream, *this);
}

GraphStreamUpdate SingleCutGenerator::get_next_edge() { return updates[edge_idx++]; }
