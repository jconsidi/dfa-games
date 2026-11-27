// LayerPermutationDFA.h

#ifndef LAYER_PERMUTATION_DFA_H
#define LAYER_PERMUTATION_DFA_H

#include <string>
#include <vector>

#include "RelabelDFA.h"

// Relabels each layer's characters by a permutation of that layer's
// alphabet: the output accepts a string t exactly when the input accepts the
// string s with t[layer] = permutations[layer][s[layer]] at every layer.
//
// A view over the input with the same states, reading each transition on
// character permutations[layer][c] from the input's transition on c. A
// permutation keeps distinct states distinct, so a minimal input gives a
// minimal output with the same number of states per layer.
class LayerPermutationDFA : public RelabelDFA
{
  static std::vector<std::vector<int>> sources_for(const dfa_shape_t&, const std::vector<std::vector<int>>&);

 public:

  LayerPermutationDFA(shared_dfa_ptr, const std::vector<std::vector<int>>&);

  // Throws std::invalid_argument unless permutations has one entry per
  // layer of shape, each a permutation of that layer's alphabet.
  static void validate(const dfa_shape_t&, const std::vector<std::vector<int>>&);
};

#endif
