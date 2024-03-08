#include "permuted_set.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

static size_t get_seed() {
  auto now = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

bool verify(size_t bits, size_t seed) {
  std::vector<bool> appeared(size_t(1) << bits, false);
  PermutedSet p(size_t(1) << bits, seed);

  for (size_t i = 0; i < size_t(1) << bits; i++) {
    size_t permute = p[i];
    if (permute > size_t(1) << bits || appeared[permute] == true) {
      return false;
    }
    appeared[permute] = true;
  }
  return true;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Incorrect number of arguments!" << std::endl;
    std::cerr << "Arguments are: num_bits" << std::endl;
    exit(EXIT_FAILURE);
  }

  size_t bits = std::stol(argv[1]);
  PermutedSet p(size_t(1) << bits, get_seed());

  auto start = std::chrono::steady_clock::now();
  size_t sum = 0;
  for (size_t i = 0; i < size_t(1) << bits; i++) {
    sum += p[i];
  }
  double latency = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::cout << "Permuted set of size " << (1 << bits) << " in "
            << latency
            << " seconds, rate = " << (1 << bits) / latency << std::endl;
  
  if (sum != ((size_t(1) << bits) - 1) * ((size_t(1) << bits)) / 2) {
    std::cerr << "ERROR: Mismatch!!!" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << std::endl;
  std::cout << "Verifying correctness of partition (even bits)" << std::endl;
  size_t seed = get_seed();
  if (verify(18, seed)) {
    std::cout << "  Success!" << std::endl;
  } else {
    std::cout << "  ERROR: Incorrect partition!" << std::endl;
    std::cout << "  18 bit universe. Seed = " << seed << std::endl;
  }

  std::cout << "Verifying correctness of partition (odd bits)" << std::endl;
  seed = get_seed();
  if (verify(19, seed)) {
    std::cout << "  Success!" << std::endl;
  } else {
    std::cout << "  ERROR: Incorrect partition!" << std::endl;
    std::cout << "  19 bit universe. Seed = " << seed << std::endl;
  }
}
