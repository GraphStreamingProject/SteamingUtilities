#include <errno.h>
#include <graph_zeppelin_common.h>
#include <string.h>
#include <unordered_map>
#include "ascii_file_stream.h"
#include "binary_file_stream.h"

#include <iostream>
#include <vector>

const std::string USAGE = "\n\
This program converts between multiple graph stream formats.\n\
USAGE:\n\
  Arguments: input_file input_type output_file output_type [--to_static] [--silent]\n\
    input_file:  The location of the file stream to convert\n\
    input_type:  The type of the input file [see types below]\n\
    output_file: Where to place the converted output stream\n\
    output_type: The type of the output file [see types below]\n\
    to_static:   [OPTIONAL] Output only the edge list for the graph state at end of input stream\n\
    silent:      [OPTIONAL] Do not print warnings\n\
\n\
  Output and input types must be one of the following\n\
    ascii_stream:        An ascii file stream that states edge update type (insert vs delete).\n\
    notype_ascii_stream: An ascii file stream that contains only edge source and destination.\n\
    binary_stream:       A binary file stream.\n\
\n\
  Additionally, optional arguments must come last.\n\
\n\
  This tool will by default map the vertices to arbitrary ids in [0,n-1]. If you want to convert\n\
  the stream from arbitrary vertex ids to [0,n-1] then use the same input_type and output_type.";

// create a stream based on parsed information
GraphStream *create_stream(std::string file_name, bool ascii, bool type, bool read) {
  GraphStream *ret;
  if (ascii) {
    ret = (GraphStream *) new AsciiFileStream(file_name, type);
  } else {
    ret = (GraphStream *) new BinaryFileStream(file_name, read);
  }
  return ret;
}

std::string print_type(UpdateType type) {
  if (type == INSERT) return "INSERT";
  if (type == DELETE) return "DELETE";
  if (type == BREAKPOINT) return "BREAKPOINT";
  return "UNKNOWN";
}

int main(int argc, char **argv) {
  if (argc < 5 || argc > 7) {
    std::cerr << "ERROR: Incorrect number of arguments. Expected [4-6] but got "
              << argc - 1 << std::endl;
    std::cerr << USAGE << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string in_file_name = argv[1];
  std::string in_file_type = argv[2];

  if (in_file_type != "notype_ascii_stream" && in_file_type != "ascii_stream" &&
      in_file_type != "binary_stream") {
    std::cerr << "ERROR: Did not recognize input_file_type: " << in_file_type << std::endl;
    std::cerr << USAGE << std::endl;
    exit(EXIT_FAILURE);
  }
  GraphStream *input = create_stream(in_file_name, in_file_type != "binary_stream",
                                     in_file_type != "notype_ascii_stream", true);

  std::string out_file_name = argv[3];
  std::string out_file_type = argv[4];

  if (out_file_type != "notype_ascii_stream" && out_file_type != "ascii_stream" &&
      out_file_type != "binary_stream") {
    std::cerr << "ERROR: Did not recognize output_file_type: " << out_file_type << std::endl;
    std::cerr << USAGE << std::endl;
    exit(EXIT_FAILURE);
  }
  GraphStream *output = create_stream(out_file_name, out_file_type != "binary_stream",
                                      out_file_type != "notype_ascii_stream", false);

  bool to_static = false;
  bool silent = false;
  for (int i = 5; i < argc; i++) {
    if (std::string(argv[i]) == "--to_static")
      to_static = true;
    else if (std::string(argv[i]) == "--silent") {
      silent = true;
    } else {
      std::cerr << "Did not recognize argument: " << argv[i]
                << " Expected '--to_static' or '--silent'" << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  node_id_t num_nodes = input->vertices();
  edge_id_t num_edges = input->edges();

  std::cout << "Parsed input stream header:" << std::endl;
  std::cout << "  Number of vertices:  " << num_nodes << std::endl;
  std::cout << "  Number of updates:   " << num_edges << std::endl;
  std::cout << "  Input stream format: " << in_file_type << std::endl;

  output->write_header(num_nodes, num_edges);

  std::unordered_map<node_id_t, node_id_t> vertex_id_map;
  node_id_t vertex_id_counter = 0;
  std::vector<std::vector<bool>> adj_mat(num_nodes);
  for (node_id_t i = 0; i < num_nodes; ++i) adj_mat[i] = std::vector<bool>(num_nodes - i);

  constexpr size_t buf_capacity = 1024;
  GraphStreamUpdate buf[buf_capacity];
  size_t true_edges = 0;

  bool processing = true;
  while (processing) {
    size_t read = input->get_update_buffer(buf, buf_capacity);
    size_t ignored = 0;
    for (size_t i = 0; i < read; i++) {
      GraphStreamUpdate upd = buf[i];
      UpdateType type = (UpdateType) upd.type;
      Edge e = upd.edge;

      if (type == BREAKPOINT) {
        processing = false;
        --read; // don't output the breakpoint!
        break;
      }

      // process the update
      if (vertex_id_map.find(e.src) == vertex_id_map.end())
        vertex_id_map[e.src] = vertex_id_counter++;
      if (vertex_id_map.find(e.dst) == vertex_id_map.end())
        vertex_id_map[e.dst] = vertex_id_counter++;
      node_id_t src = std::min(vertex_id_map[e.src], vertex_id_map[e.dst]);
      node_id_t dst = std::max(vertex_id_map[e.src], vertex_id_map[e.dst]);

      if (src == dst) {
        if (!silent)
          std::cerr << "WARNING: Dropping self loop edge " << src << ", " << dst << std::endl;
        ++ignored;
        continue;
      }

      if (!silent && in_file_type != "notype_ascii_stream" && type != QUERY && type != adj_mat[src][dst - src - 1]) {
        std::cerr << "WARNING: update " << print_type(type) << " " << e.src << " " << e.dst;
        std::cerr << " is double insert or delete before insert." << std::endl;
      }

      buf[i].type = adj_mat[src][dst - src - 1];
      buf[i].edge.src = src;
      buf[i].edge.dst = dst;
      adj_mat[src][dst - src - 1] = !adj_mat[src][dst - src - 1];

      // shift past ignored if necessary
      buf[i - ignored] = buf[i];
    }

    // copy buffer to output stream
    if (!to_static)
      output->write_updates(buf, read - ignored);

    true_edges += read - ignored;

    if (true_edges % (buf_capacity * 10000) == 0) {
      std::cout << "Processed: " << true_edges << " edges           \r"; fflush(stdout);
    }
  }

  if (to_static) {
    true_edges = 0; // only count edges in final graph in static stream
    size_t buf_size = 0;
    for (node_id_t src = 0; src < num_nodes; src++) {
      for (node_id_t dst = 0; dst < adj_mat[src].size(); dst++) {
        true_edges += adj_mat[src][dst];
      }
    }
    output->write_header(num_nodes, true_edges);
    for (node_id_t src = 0; src < num_nodes; src++) {
      for (node_id_t dst = 0; dst < adj_mat[src].size(); dst++) {
        if (adj_mat[src][dst]) {
          buf[buf_size++] = {INSERT, {src, src + dst + 1}};
          if (buf_size >= buf_capacity) {
            output->write_updates(buf, buf_size);
            buf_size = 0;
          }
        }
      }
    }
    if (buf_size > 0)
      output->write_updates(buf, buf_size);
  } else {
    // update header to reflect true edges
    output->write_header(num_nodes, true_edges);
  }

  std::cout << "Done                            " << std::endl;

  delete input;
  delete output;
}
