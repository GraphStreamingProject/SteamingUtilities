#include <graph_zeppelin_common.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>
#include <queue>
#include <cstdlib>
#include <chrono>
#include <iomanip>

#include "binary_file_stream.h"
#include "permuted_set.h"

const std::string USAGE = "\n\
=== PROGRAM DESCRIPTION AND USAGE ===\n\
This program takes as input a graph stream and outputs a queryified version of that graph. The\n\
input graph must be a BinaryFileStream, if your stream is a different type the\n\
'stream_file_converter' tool can be used to switch it to a BinaryFileStream. The input graph need\n\
not be static, but this tool can only add additional queries. \n\
USAGE:\n\
  Arguments: input_file output_file density burst_period_min burst_period_max [--seed seed]\n\
    input_file:        The location of the file stream to convert. MUST be a BinaryFileStream.\n\
    output_file:       Where to place the queryified BinaryFileStream.\n\
    density:           Percentage of stream operations that should be queries.\n\
    burst_period_min:  The burst period is the number updates between two bursts of queries. \n\
                       The period is uniformly chosen for each burst between the min and max \n\
                       value. The number of queries in the burst is adjusted appropriately to \n\
                       maintain the desired query density. \n\
    burst_period_max:  The upper bound for the random burst period. \n\
    seed seed:         [OPTIONAL] Define the seed to random number generation. If not defined one is\n\
                       chosen randomly.\n";

const bool verbose = false;

size_t generate_seed() {
  auto ts = std::chrono::steady_clock::now();
  size_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(ts.time_since_epoch()).count();

  std::mt19937_64 gen(ns);
  return gen();
}

// return the directory in which a file can be found or "." if none specified
std::string get_file_directory(std::string file_name) {
  size_t found = file_name.rfind('/');

  if (found == std::string::npos) {
    return std::string(".");
  }
  return file_name.substr(0, found);
}

void copy_file(std::string src_file_name, std::string dst_file_name) {
  std::ifstream input(src_file_name, std::ios::binary);
  std::ofstream output(dst_file_name, std::ios::binary | std::ios::trunc);

  output << input.rdbuf();
}

// creates a random Query
GraphStreamUpdate create_rand_query(node_id_t num_vertices, std::mt19937_64 &gen) {
  node_id_t src = 0;
  node_id_t dst = 0;
  while (src == dst) {
    src = gen() % num_vertices;
    dst = gen() % num_vertices;
  }
  return {QUERY, {src, dst}};
}

int main(int argc, char **argv) {
  if (argc < 6) {
    std::cerr << "ERROR: Incorrect number of arguments. Expected at least 5 but got "
              << argc - 1 << std::endl;
    std::cerr << USAGE << std::endl;
    exit(EXIT_FAILURE);
  }

  // parse the main arguments
  std::string in_file_name = argv[1];
  std::string out_file_name = argv[2];
  double density = atof(argv[3]);
  size_t burst_min = atoi(argv[4]);
  size_t burst_max = atoi(argv[5]);

  // parse optional seed argument
  size_t seed = generate_seed();
  int arg = 6;
  while (arg < argc) {
    std::string arg_str = argv[arg++];
    if (arg_str == "--seed") {
      if (arg + 1 > argc) {
        std::cerr << "ERROR: --seed requires the 'seed' argument!" << std::endl;
        std::cerr << USAGE << std::endl;
        exit(EXIT_FAILURE);
      }
      std::string seed_str = argv[arg++];
      seed = std::stol(seed_str);
    } else {
        std::cerr << "ERROR: Could not parse argument: " << arg_str << std::endl;
        std::cerr << USAGE << std::endl;
        exit(EXIT_FAILURE);
    }
  }

  std::cout << std::setprecision(2) << std::fixed;
  std::cout << "Queryifying input:    " << in_file_name << std::endl;
  std::cout << "Output file name:     " << out_file_name << std::endl;
  std::cout << "Seed:                 " << seed << std::endl;
  std::cout << "Query Density:        " << density << std::endl;
  std::cout << "Burst Period Min:     " << burst_min << std::endl;
  std::cout << "Burst Period Max:     " << burst_max << std::endl;
  std::cout << std::endl;

  BinaryFileStream *input = new BinaryFileStream(in_file_name, true);
  BinaryFileStream *output = new BinaryFileStream(out_file_name, false);

  std::mt19937_64 gen(seed * 53);

  // Some buffers for reading and writing to our streams
  size_t buffer_size = 4096;
  GraphStreamUpdate input_updates[buffer_size];
  GraphStreamUpdate output_updates[buffer_size];
  size_t input_pos = 0;
  size_t output_pos = 0;

  // Pass through the stream adding queries at some points
  edge_id_t updates_remain = input->edges();
  size_t total_queries = 0;
  size_t buffer_updates = input->get_update_buffer(input_updates, buffer_size);
  size_t burst_period = gen() % (burst_max-burst_min) + burst_min;
  size_t burst_updates = 0;

  std::cout << "Creating Queryified Stream ..." << std::endl;
  for (; updates_remain > 0; updates_remain--) {
    GraphStreamUpdate update = input_updates[input_pos++];
    output_updates[output_pos++] = update;
    burst_updates++;
    // Deal with input/output stream buffering
    if (input_pos >= buffer_updates) {
      buffer_updates = input->get_update_buffer(input_updates, buffer_size);
      input_pos = 0;
    }
    if (output_pos >= buffer_size) {
      output->write_updates(output_updates, output_pos);
      output_pos = 0;
    }
    // Add some queries if the burst period number of updates have passed
    if (burst_updates == burst_period) {
      size_t num_queries = (density * burst_updates) / (1. - density); // This expression gives density % queries
      if (verbose) std::cout << burst_updates << " UPDATES OCCURED, NOW ADDING " << num_queries << " QUERIES" << std::endl;
      for (size_t i = 0; i < num_queries; i++) {
        output_updates[output_pos++] = create_rand_query(input->vertices(), gen);
        // Deal with input/output stream buffering
        if (output_pos >= buffer_size) {
          output->write_updates(output_updates, output_pos);
          output_pos = 0;
        }
      }
      total_queries += num_queries;
      burst_period = gen() % (burst_max-burst_min) + burst_min;
      burst_updates = 0;
    }
  }
  // Add a final burst of queries
  size_t num_queries = (density * burst_updates) / (1. - density); // This expression gives density % queries
  if (verbose) std::cout << burst_updates << " UPDATES OCCURED, NOW ADDING " << num_queries << " QUERIES" << std::endl;
  for (size_t i = 0; i < num_queries; i++) {
    output_updates[output_pos++] = create_rand_query(input->vertices(), gen);
    // Deal with input/output stream buffering
    if (output_pos >= buffer_size) {
      output->write_updates(output_updates, output_pos);
      output_pos = 0;
    }
  }
  total_queries += num_queries;
  output->write_updates(output_updates, output_pos);

  // write the header to the output stream
  output->write_header(input->vertices(), input->edges() + total_queries);

  std::cout << std::endl;
  std::cout << "Created stream " << out_file_name << std::endl;
  std::cout << "Vertices:   " << input->vertices() << std::endl;
  std::cout << "Updates:    " << input->edges() << std::endl;
  std::cout << "Queries:    " << total_queries << std::endl;
}
