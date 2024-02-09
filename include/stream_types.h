#pragma once

#include <graph_zeppelin_common.h>

// Is a stream update an insertion or a deletion
// BREAKPOINT: special type that indicates that a break point has been reached
// a break point may be either the end of the stream or the index of a query
enum UpdateType {
  INSERT = 0,
  DELETE = 1,
  BREAKPOINT = 2
};

struct Edge {
  node_id_t src = 0;
  node_id_t dst = 0;

  bool operator< (const Edge&oth) const {
    if (src == oth.src)
      return dst < oth.dst;
    return src < oth.src;
  }
  bool operator== (const Edge&oth) const {
    return src == oth.src && dst == oth.dst;
  }
};

#pragma pack(push,1)
struct GraphStreamUpdate {
  uint8_t type;
  Edge edge;
};
#pragma pack(pop)

static constexpr edge_id_t END_OF_STREAM = (edge_id_t) -1;

// Enum that defines the types of streams
enum StreamType {
  BinaryFile,
  AsciiFile,
};
