#include "static_erdos_generator.h"
#include "ascii_file_stream.h"
#include "binary_file_stream.h"

StaticErdosGenerator::StaticErdosGenerator(node_id_t num_vertices, double density, size_t seed)
    : num_vertices(num_vertices),
      density(density),
      seed(seed),
      total_edges(size_t(num_vertices) * (size_t(num_vertices) - 1) / 2 * density),
      permute(num_vertices * num_vertices - 1, seed) {

  if (log2(num_vertices) - size_t(log2(num_vertices)) != 0) {
    throw StreamException("StaticErdosGenerator: Number of vertices must be a power of 2!");
  }
  v_bits = log2(num_vertices);
}

void write_to_file(GraphStream *stream, StaticErdosGenerator &gen) {
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
}

void StaticErdosGenerator::to_binary_file(std::string file_name) {
  BinaryFileStream output_stream(file_name, false);
}
void StaticErdosGenerator::to_ascii_file(std::string file_name) {
  AsciiFileStream output_stream(file_name, false);
}

static Edge extract_edge(size_t v_bits, size_t packed_edge) {
  Edge e{node_id_t(packed_edge >> v_bits) << 1,
          node_id_t(packed_edge & ((size_t(1) << v_bits) - 1))};
  if (e.src > e.dst && e.dst % 2 == 0) {
    ++e.src; ++ e.dst;
  }
  return e;
}

GraphStreamUpdate StaticErdosGenerator::get_next_edge() {
  Edge e = extract_edge(v_bits, permute[edge_idx + skip]);
  
  while (e.src == e.dst) {
    ++skip;
    e = extract_edge(v_bits, permute[edge_idx + skip]);
  }
  ++edge_idx;

  return {INSERT, e};
}
