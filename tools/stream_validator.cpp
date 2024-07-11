#include <binary_file_stream.h>
#include <ascii_file_stream.h>
#include <vector>

std::string type_string(uint8_t type) {
  if (type == INSERT)
    return "INSERT";
  else if (type == DELETE)
    return "DELETE";
  else
    return "BREAKPOINT";
}

void err_edge(Edge edge, UpdateType u, size_t e) {
  std::cerr << "ERROR: edge idx: " << e << "=(" << edge.src << "," << edge.dst << "), "
            << type_string(u) << std::endl;
}

// Check that a stream is formatted correctly. Check that types are correct and
// node ids are in range.

int main(int argc, char **argv) {
  if (argc < 3 || argc > 4) {
    std::cout << "Incorrect Number of Arguments!" << std::endl;
    std::cout << "Arguments: stream_type stream_file [cumulative_file]" << std::endl;
    exit(EXIT_FAILURE);
  }
  
  std::string stream_type = argv[1];
  std::string stream_file = argv[2];
  std::string cumul_file;
  if (argc == 4)
    cumul_file = argv[3];

  GraphStream *stream;
  if (stream_type == "binary") {
    stream = new BinaryFileStream(stream_file);
  } else if (stream_type == "ascii") {
    stream = new AsciiFileStream(stream_file);
  } else {
    throw StreamException("stream_validator: Unknown stream_type. Should be 'binary' or 'ascii'");
  }

  node_id_t nodes = stream->vertices();
  size_t edges    = stream->edges();

  std::cout << "Attempting to validate stream " << argv[1] << std::endl;
  std::cout << "Number of nodes   = " << nodes << std::endl;
  std::cout << "Number of updates = " << edges << std::endl;

  // create an adjacency matrix
  std::vector<std::vector<bool>> adj_mat(nodes);
  for (node_id_t i = 0; i < nodes; i++) {
    adj_mat[i] = std::vector<bool>(nodes - i - 1, false);
  }

  // validate the type, src, and dst of each update in the stream
  bool err = false;
  for (size_t e = 0; e < edges; e++) {
    GraphStreamUpdate upd;
    try {
      stream->get_update_buffer(&upd, 1);
    } catch (...) {
      std::cerr << "ERROR: Could not get edge at index: " << e << std::endl;
      err = true;
      throw;
    }
    Edge edge = upd.edge;
    UpdateType u = static_cast<UpdateType>(upd.type);

    if (edge.src == edge.dst) {
      err_edge(edge, u, e);
      std::cerr << "       Cannot have equal src and dst" << std::endl;
      err = true;
      continue;
    }

    if (u == QUERY) continue;
    
    node_id_t src = std::min(edge.src, edge.dst);
    node_id_t local_dst = std::max(edge.src, edge.dst) - src - 1;
    if (adj_mat[src][local_dst] != u) {
      err_edge(edge, u, e);
      std::cerr << "       Incorrect type! Expect: " << type_string(adj_mat[src][local_dst])
                << std::endl;
      err = true;
    }
    adj_mat[src][local_dst] = !adj_mat[src][local_dst];

    if (edge.src >= nodes || edge.dst >= nodes) {
      err_edge(edge, u, e);
      std::cerr << "       src or dst out of bounds." << std::endl;
      err = true;
    }
    if (e % 1000000000 == 0 && e != 0) {
      std::cout << e << "\r"; fflush(stdout);
    } 
  }
  std::cout << std::endl;

  if (!err) std::cout << "Stream validated!" << std::endl;
  if (err) {
    std::cout << "ERROR: Stream invalid!" << std::endl;
    delete stream;
    exit(EXIT_FAILURE);
  }

  // if we have a cumulative file. Parse it into an adjacency matrix with a AsciiFileStream and
  // compare the two adjacency lists
  if (argc == 4) {
    AsciiFileStream cumul_stream(cumul_file, false);
    node_id_t cumul_nodes = cumul_stream.vertices();
    edge_id_t cumul_edges = cumul_stream.edges();

    if (cumul_nodes != nodes) {
      throw StreamException("stream_validator: Number of nodes do not match stream and cumul");
    }

    // create a cumulative adjacency matrix
    std::vector<std::vector<bool>> cumul_adj(nodes);
    for (node_id_t i = 0; i < nodes; i++) {
      cumul_adj[i] = std::vector<bool>(nodes - i - 1, false);
    }

    for (size_t e = 0; e < cumul_edges; e++) {
      GraphStreamUpdate upd;
      cumul_stream.get_update_buffer(&upd, 1);

      node_id_t src = std::min(upd.edge.src, upd.edge.dst);
      node_id_t local_dst = std::max(upd.edge.src, upd.edge.dst) - src - 1;
      if (cumul_adj[src][local_dst]) {
        throw StreamException("stream_validator: Edges must appear only once in cumul file!");
      }
      cumul_adj[src][local_dst] = true;
    }

    for (node_id_t s = 0; s < nodes; s++) {
      for (node_id_t d = 0; d < adj_mat[s].size(); d++) {
        if (adj_mat[s][d] != cumul_adj[s][d]) {
          std::cerr << "ERROR: Cumul mismatch on edge (" << s << "," << s + d + 1 << ")"
                    << std::endl;
          err = true;
        }
      }
    }

    if (!err) std::cout << "Resulting graph matches cumulative file!" << std::endl;
    if (err) std::cout << "ERROR: Resulting graph does not match cumulative file!" << std::endl;
  }

  delete stream;
}

