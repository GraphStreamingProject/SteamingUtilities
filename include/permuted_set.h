#pragma once
#include <xxhash.h>
#include <cmath>
static const auto& hash = XXH3_64bits_withSeed;

/* Permute the set of integers [0, 2^i]
 *  uses XXH3 for hashing
 *  Uses Permute(L|R) = R|(L XOR H(R)) (input is split into two equal bit length subnumbers L,R)
 *  Compose Permute 3 times to get a pseudo-random permutation
 *
 *  Based upon the paper "How to Construct Pseudorandom Permutations from Pseudorandom Functions"
 *    url: https://inst.eecs.berkeley.edu/~cs276/fa20/notes/Luby_Rackoff_paper.pdf
 *
 *  We make a modification to deal with an odd number of bits without having to round up to the
 *  next power of 2. 
 *    Rounding up by at most factor 2 is much more acceptable than factor 4 and at least 2
 *    Modification if odd bits:
 *      * bit b is always unchanged (ignored bit)
 *      * Create two functions H and G
 *      * H splits the input into L|R|b
 *      * G splits the input into L|b|R
 *      * Both functions permute L and R as normal and compose the result with the ignored bit b
 *      * Compose H and G to create a pseudo-random permutation on odd bits
 */

class PermutedSet {
 private:
  size_t n;
  size_t L_shift;
  size_t HR_mask;
  size_t GR_mask;
  size_t Gb_mask;
  size_t hash_seeds[2];
  size_t is_odd; // 1 if bits odd, 0 if even

  // Split is i = L | R | b
  inline size_t H(size_t i, size_t h) {
    size_t L = i >> L_shift;
    size_t R = (i & HR_mask) >> is_odd;
    size_t b = is_odd & i;

    size_t hash_value = hash(&R, sizeof(R), hash_seeds[h]) & GR_mask;
    size_t temp = R;
    R = hash_value ^ L;
    L = temp;

    return (L << L_shift) | (R << is_odd) | b;
  }

  // Split is i = L | b | R
  inline size_t G(size_t i, size_t h) {
    size_t L = i >> L_shift;
    size_t R = i & GR_mask;
    size_t b = i & Gb_mask;

    size_t hash_value = hash(&R, sizeof(R), hash_seeds[h]) & GR_mask;
    size_t temp = R;
    R = hash_value ^ L;
    L = temp;

    return (L << L_shift) | R | b;
  }

 public:
  PermutedSet(size_t n, size_t seed) {
    hash_seeds[0] = seed * 3;
    hash_seeds[1] = hash_seeds[0] * 5;

    size_t bits = ceil(log2(n));
    is_odd = bits % 2 == 1;
    
    L_shift = (bits / 2) + is_odd;
    HR_mask = (1 << ((bits / 2) + is_odd)) - 1;
    GR_mask = (1 << (bits / 2)) - 1;
    Gb_mask = (is_odd << (bits/2));
  }

  size_t operator[](size_t i) {
    return H(G(i, 0), 1);
  }
};
