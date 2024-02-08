# StreamingUtilities
A library for formatting, parsing and generating graph streams. 

## Stream format
The graph stream interface is defined in `include/graph_stream.h` and two example stream formats can be found in `include/binary_file_stream.h` and `include/ascii_file_stream.h`.

Additional stream formats can be defined in user code by inheriting from the `GraphStream` class.

## Generation
The library includes classes for either dynamic (insert and delete) or static (insert only) stream generation. The classes are listed below.
### StaticErdosGenerator
Quickly generates a static stream that defines an Erdos-Renyi graph. The input to this generator is the number of vertices (must be a power of two) and the density of the desired graph.
