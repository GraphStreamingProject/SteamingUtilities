#include <graph_zeppelin_common.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>
#include <cstdlib>
#include <chrono>
#include <iomanip>

#include "ascii_file_stream.h"
#include "binary_file_stream.h"
#include "permuted_set.h"

// TODO: How do preprocessed and shuffle interact
const std::string USAGE = "\n\
=== PROGRAM DESCRIPTION AND USAGE ===\n\
This program takes as input a graph stream and outputs a streamified version of that graph. The\n\
input graph must be a BinaryFileStream, if your stream is a different type the\n\
'stream_file_converter' tool can be used to switch it to a BinaryFileStream. The input graph need\n\
not be static, but this tool can only add additional dynamic edges. To remove existing dynamic\n\
edges make the graph static using the 'stream_file_converter' tool.\n\
USAGE:\n\
  Arguments: input_file output_file density[+] [--extra percent] [--preprocessed]\n\
             [--shuffle] [--seed seed]\n\
    input_file:    The location of the file stream to convert. MUST be a BinaryFileStream.\n\
    output_file:   Where to place the streamified BinaryFileStream.\n\
    density:       One or more density checkpoints, the stream will move from one density\n\
                   checkpoint to another performing insert and deletes as necessary to reach the\n\
                   desired density.\n\
    extra percent: [OPTIONAL] How many additional random inserts to inject between density\n\
                   checkpoints. This is expressed as a percentage of the updates performed to\n\
                   reach a density checkpoint. If 0 [the default], then only the necessary\n\
                   inserts or deletes are performed for reaching the desired density. If 200, 2x\n\
                   the required updates are performed. Any inserted edges must be deleted back\n\
                   out so the total number of extra updates is a factor 2 greater.\n\
    preprocessed:  [OPTIONAL] This stream is created assuming that the graph defined by the\n\
                   input stream has already been loaded by the system. That is, we assume the\n\
                   output stream begins after a density checkpoint of 100.\n\
    shuffle:       [OPTIONAL] If this flag is present, perform streamifying upon shuffled input.\n\
    seed seed:     [OPTIONAL] Define the seed to random number generation. If not defined one is\n\
                   chosen randomly.\n\
\n\
  Density + Optional Arg Examples :  Explanation\n\
    100 0 100                     :  We insert the stream, delete it back out, then reinsert.\n\
    100 --extra 400               :  Insert the stream, but also 400% random insert/delete pairs.\n\
    20 40 60 80 100 --extra 200   :  Insert 20% of the stream and 20% random insert/delete pairs.\n\
                                  :  Repeat until stream finished.\n\
    100 --shuffle                 :  Shuffle the stream but do not perfom any streamifying.\n\
    100 --preprocessed            :  Inverts type flags. After preprocessing, stream ends empty.\n\
\n\
  Density and optional arguments must all appear after the file arguments.\n";

size_t generate_seed() {
  auto ts = std::chrono::steady_clock::now();
  size_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(ts.time_since_epoch()).count();

  std::mt19937_64 gen(ns);
  return gen();
}

std::string print_type(UpdateType type) {
  if (type == INSERT) return "INSERT";
  if (type == DELETE) return "DELETE";
  if (type == BREAKPOINT) return "BREAKPOINT";
  return "UNKNOWN";
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

size_t get_upd_buffer_no_break(GraphStream *stream, GraphUpdate *buf, size_t buf_size) {
  size_t read = stream->get_update_buffer(buf, buf_size);

  if (read > 0 && buf[read - 1].type == BREAKPOINT) {
    read--;
  }

  return read;
}

// take a binary stream and place the shuffled version in a file of a given name
void shuffle_stream(size_t seed, std::string temp_file_name) {
  std::cout << "Shuffling Stream..." << std::endl;
  BinaryFileStream shuf_stream(temp_file_name, false);

  edge_id_t num_edges = shuf_stream.edges();
  edge_id_t buffer_size = std::min(edge_id_t(1000000), num_edges);

  std::cout << "shuffled stream edges = " << num_edges << std::endl;

  GraphUpdate *upd_buf1 = new GraphUpdate[buffer_size];
  GraphUpdate *upd_buf2 = new GraphUpdate[buffer_size];

  std::mt19937_64 rand_gen(seed * 107);

  // repeatedly shuffle a buffer of updates and then place them at a random offset
  // do this twice for sufficient randomness
  for(int r = 0; r < 2; r++) {
    for (edge_id_t e = 0; e < num_edges; e += buffer_size) {
      // read and shuffle updates at beginning of stream
      shuf_stream.seek(e);
      size_t read = get_upd_buffer_no_break(&shuf_stream, upd_buf1, buffer_size);
      std::shuffle(&upd_buf1[e], &upd_buf1[e + read], rand_gen);

      // choose a position where we can place the buffer
      edge_id_t write_idx = rand_gen() % (num_edges - buffer_size + 1);
      std::cout << "placing shuffled updates at index: " << write_idx << std::endl;
      
      // read contents at chosen index, and write edges there
      shuf_stream.seek(write_idx);
      shuf_stream.get_update_buffer(upd_buf2, read);
      shuf_stream.seek(write_idx);
      shuf_stream.write_updates(upd_buf1, read);

      // shuffle and place copied updates
      std::shuffle(&upd_buf2[e], &upd_buf2[e + read], rand_gen);
      std::cout << "placing displaced updates at index: " << e << std::endl;
      shuf_stream.seek(e);
      shuf_stream.write_updates(upd_buf2, read);
    }
  }
  delete upd_buf1;
  delete upd_buf2;
}

edge_id_t calc_streamy_edges(edge_id_t static_edges, std::vector<double> &density_checkpoints,
                             double factor_adtl_updates, bool preprocess) {
  edge_id_t stream_edges = 0;
  edge_id_t true_stream;
  if (preprocess) {
    true_stream = (100 - density_checkpoints[0]) * static_edges;
  } else {
    true_stream = density_checkpoints[0] * static_edges;
  }
  stream_edges += true_stream + 2 * edge_id_t(true_stream * factor_adtl_updates);
  
  for (size_t i = 1; i < density_checkpoints.size(); i++) {
    true_stream = abs(density_checkpoints[i] - density_checkpoints[i - 1]) * static_edges;
    stream_edges += true_stream + 2 * edge_id_t(true_stream * factor_adtl_updates);
  }

  return stream_edges;
}

// creates a random Edge
Edge create_rand_update(node_id_t num_vertices, std::mt19937_64 &gen) {
  
  node_id_t src = 0;
  node_id_t dst = 0;
  while (src == dst) {
    src = gen() % num_vertices;
    dst = gen() % num_vertices;
  }

  return {src, dst};
}

void add_updates_for_checkpoint(size_t seed, BinaryFileStream *input, BinaryFileStream *output,
                                std::vector<std::vector<bool>> &adj_mat,
                                double current_stream_density, double goal_stream_density,
                                double factor_adtl_updates) {
  std::cout << "DENSITY CHECKPOINT: " << current_stream_density << " -> " << goal_stream_density
            << std::endl;

  // establish static graph edge start/end
  edge_id_t start_edge_idx = current_stream_density * input->edges();
  edge_id_t end_edge_idx = goal_stream_density * input->edges();
  bool delete_static = goal_stream_density < current_stream_density;
  edge_id_t stream_edges_remain;

  if (delete_static) {
    // we 'reverse' the stream to delete out (goal -> current)
    input->seek(end_edge_idx);
    input->set_break_point(start_edge_idx);
    stream_edges_remain = start_edge_idx - end_edge_idx;

  } else {
    input->set_break_point(end_edge_idx);
    stream_edges_remain = end_edge_idx - start_edge_idx;

  }
  // Some variables to handle what edge updates we output
  edge_id_t extra_write_remain = stream_edges_remain * factor_adtl_updates;
  edge_id_t extra_remove_remain = stream_edges_remain * factor_adtl_updates;
  edge_id_t extra_remove_avail = 0;
  edge_id_t total_edges_remain = stream_edges_remain + extra_write_remain + extra_remove_remain;
  std::cout << "  Checkpoint edges = " << total_edges_remain
            << " (stream edges = " << stream_edges_remain
            << " extra ins/del pairs = " << extra_remove_remain << ")" << std::endl;

  // create two random number generators with the same seed so we can remove the effects
  // of the first using the second.
  std::mt19937_64 edge_gen_add(seed * 53);
  std::mt19937_64 edge_gen_remove(seed * 53);

  // random number generator for choosing when to add/remove/stream
  std::mt19937_64 edge_type_choice(seed * 3);

  // stream management variables
  size_t buffer_size = 4096;
  GraphUpdate input_updates[buffer_size];
  GraphUpdate output_updates[buffer_size];
  size_t input_pos = 0;
  size_t output_pos = 0;
  size_t updates = input->get_update_buffer(input_updates, buffer_size);

  for (; total_edges_remain > 0; total_edges_remain--) {
    // choose which type of edge update we perform, stream/extra-insert/extra-delete
    edge_id_t valid_choices = stream_edges_remain + extra_write_remain + extra_remove_avail;
    edge_id_t choice = edge_type_choice() % valid_choices;
    Edge edge;
    if (choice < stream_edges_remain) {
      // stream-edge
      UpdateType type = static_cast<UpdateType>(input_updates[input_pos].type);
      edge = input_updates[input_pos++].edge;
      if (type == BREAKPOINT) {
        std::cerr << "ERROR: Encountered breakpoint during checkpoint processing!" << std::endl;
        exit(EXIT_FAILURE);
      }
      --stream_edges_remain;
    } else if (choice < stream_edges_remain + extra_write_remain) {
      // extra-write
      edge = create_rand_update(input->vertices(), edge_gen_add);
      --extra_write_remain;
      ++extra_remove_avail;
    } else {
      // extra-delete
      edge = create_rand_update(input->vertices(), edge_gen_remove);
      --extra_remove_remain;
      --extra_remove_avail;
    }

    // identify the correct type of the edge and place it into the output stream
    node_id_t src = std::min(edge.src, edge.dst);
    node_id_t local_dst = std::max(edge.src, edge.dst) - src - 1;

    // perform some quick error checking
    if (edge.src >= input->vertices() || edge.dst >= input->vertices() || edge.src == edge.dst) {
      std::cerr << "ERROR: Bad edge encountered (" << edge.src << ", " << edge.dst << ")"
                << std::endl;
      exit(EXIT_FAILURE);
    }


    bool present = adj_mat[src][local_dst];
    adj_mat[src][local_dst] = !adj_mat[src][local_dst];
    output_updates[output_pos++] = {(UpdateType) present, edge};

    // deal with input/output stream buffering
    if (input_pos >= updates) {
      updates = input->get_update_buffer(input_updates, buffer_size);
      input_pos = 0;
    }
    if (output_pos >= buffer_size) {
      output->write_updates(output_updates, output_pos);
      output_pos = 0;
    }
  }

  if (extra_write_remain + extra_remove_remain + extra_remove_avail != 0) {
    std::cerr << "Did not reach 0 updates remaining of each type!" << std::endl;
    std::cerr << extra_write_remain << ", " << extra_remove_remain << ", " << extra_remove_avail
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (output_pos > 0) {
    output->write_updates(output_updates, output_pos);
    output_pos = 0;
  }

  if (delete_static) {
    input->seek(end_edge_idx);
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cerr << "ERROR: Incorrect number of arguments. Expected at least 4 but got "
              << argc - 1 << std::endl;
    std::cerr << USAGE << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string in_file_name = argv[1];
  std::string out_file_name = argv[2];

  // parse density and optional arguments
  double extra_percent = 0;
  bool preprocessed = false;
  bool shuffle = false;
  size_t seed = generate_seed();
  std::vector<double> density_checkpoints;

  int arg = 3;
  size_t num_chars;
  while (arg < argc) {
    std::string arg_str = argv[arg++];

    if (arg_str == "--extra") {
      if (arg + 1 > argc) {
        std::cerr << "ERROR: --extra requires the 'percent' argument!" << std::endl;
        std::cerr << USAGE << std::endl;
        exit(EXIT_FAILURE);
      }

      std::string percent = argv[arg++];
      extra_percent = double(std::stol(percent, &num_chars)) / 100;

      if (num_chars != percent.size()) {
        std::cerr << "ERROR: Could not parse '--extra' argument: " << percent << std::endl;
        std::cerr << USAGE << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (arg_str == "--preprocessed") {
      preprocessed = true;
    } else if (arg_str == "--shuffle") {
      shuffle = true;
    } else if (arg_str == "--seed") {
      if (arg + 1 > argc) {
        std::cerr << "ERROR: --seed requires the 'seed' argument!" << std::endl;
        std::cerr << USAGE << std::endl;
        exit(EXIT_FAILURE);
      }

      std::string seed_str = argv[arg++];
      seed = std::stol(seed_str);
      if (num_chars != seed_str.size()) {
        std::cerr << "ERROR: Could not parse '--seed' argument: " << seed_str << std::endl;
        std::cerr << USAGE << std::endl;
        exit(EXIT_FAILURE);
      }
    } else {
      // verify that the input is a density checkpoint (i.e. integer >= 0)
      density_checkpoints.push_back(double(std::stol(arg_str, &num_chars)) / 100);
      if (num_chars != arg_str.size()) {
        std::cerr << "ERROR: Could not parse density checkpoint: " << arg_str << std::endl;
        std::cerr << USAGE << std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  if (density_checkpoints.size() == 0) {
    std::cerr << "ERROR: Must specify at least 1 density checkpoint!" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << std::setprecision(2) << std::fixed;
  std::cout << "Streamifying input:   " << in_file_name << std::endl;
  std::cout << "Output file name:     " << out_file_name << std::endl;
  std::cout << "Seed:                 " << seed << std::endl;
  std::cout << "Shuffle input:        " << (shuffle ? "True" : "False") << std::endl;
  std::cout << "Begins preprocessed:  " << (preprocessed ? "True" : "False") << std::endl;
  std::cout << "Extra updates factor: " << extra_percent << std::endl;
  std::cout << "Density checkpoints: ";
  for (auto density : density_checkpoints) {
    std::cout << " " << density;
  }
  std::cout << std::endl;

  BinaryFileStream *input = new BinaryFileStream(in_file_name, true);
  BinaryFileStream *output = new BinaryFileStream(out_file_name, false);

  // if we need a temporary file, this is it
  std::string output_dir = get_file_directory(out_file_name);
  std::string temp_file_name = output_dir + "/temp_shuf_stream";

  if (shuffle) {
    std::cout << "Shuffling in temporary stream file: " << temp_file_name << std::endl;
    std::cout << "Copy stream..." << std::endl;
    // copy the input stream to the temporary file
    copy_file(in_file_name, temp_file_name);

    // shuffle the stream
    shuffle_stream(seed, temp_file_name);

    // set input stream to the generated temporary file
    delete input;
    input = new BinaryFileStream(temp_file_name, true);
  }

  // calculate how many total edges the streamified stream will have
  size_t streamy_edges =
      calc_streamy_edges(input->edges(), density_checkpoints, extra_percent, preprocessed);

  // write the header to the output stream
  output->write_header(input->vertices(), streamy_edges);

  // create an adjacency matrix for storing the state of the graph
  std::vector<std::vector<bool>> adj_mat(input->vertices());
  for (node_id_t i = 0; i < input->vertices(); i++) {
    adj_mat[i] = std::vector<bool>(input->vertices() - i - 1, false);
  }

  // streamify updates and place in the output stream
  add_updates_for_checkpoint(seed, input, output, adj_mat, preprocessed ? 100 : 0,
                             density_checkpoints[0], extra_percent);
  for (size_t d = 1; d < density_checkpoints.size(); d++) {
    add_updates_for_checkpoint(seed * (d+1), input, output, adj_mat, density_checkpoints[d - 1],
                               density_checkpoints[d], extra_percent);
  }

  if (shuffle) {
    // delete the temporary file
    std::remove(temp_file_name.c_str());
  }
}
