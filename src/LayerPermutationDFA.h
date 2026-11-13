// LayerPermutationDFA.h

#ifndef LAYER_PERMUTATION_DFA_H
#define LAYER_PERMUTATION_DFA_H

#include <string>
#include <vector>

#include "DFA.h"

// Relabels each layer's characters by a permutation of that layer's
// alphabet: the output accepts a string t exactly when the input accepts the
// string s with t[layer] = permutations[layer][s[layer]] at every layer.
//
// Built in one pass over the input, keeping every state and its number and
// moving each transition from character c to permutations[layer][c]. A
// permutation keeps distinct states distinct, so a minimal input gives a
// minimal output with the same number of states per layer.
class LayerPermutationDFA : public DFA
{
 public:
  LayerPermutationDFA(const DFA&, const std::vector<std::vector<int>>&);

  // Throws std::invalid_argument unless permutations has one entry per
  // layer of shape, each a permutation of that layer's alphabet.
  static void validate(const dfa_shape_t&, const std::vector<std::vector<int>>&);

  // A short name for the permutations, for cache file names: each run of
  // layers with the same permutation as "a-b-cxN", runs joined by "_".
  // Throws std::length_error if that is too long for a file name.
  static std::string cache_key(const dfa_shape_t&, const std::vector<std::vector<int>>&);
};

#endif
