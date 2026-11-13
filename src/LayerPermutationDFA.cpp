// LayerPermutationDFA.cpp

#include "LayerPermutationDFA.h"

#include <format>
#include <stdexcept>

LayerPermutationDFA::LayerPermutationDFA(const DFA& dfa_in, const std::vector<std::vector<int>>& permutations)
  : DFA(dfa_in.get_shape())
{
  validate(get_shape(), permutations);

  int ndim = get_shape_size();

  dfa_in.mmap();

  if(dfa_in.get_initial_state() < 2)
    {
      // accept all or reject all, which every permutation maps to itself.
      this->set_initial_state(dfa_in.get_initial_state());
      return;
    }

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = dfa_in.get_layer_shape(layer);
      const std::vector<int>& permutation = permutations[size_t(layer)];

      build_layer(layer, dfa_in.get_layer_size(layer), [&](dfa_state_t old_state_index, dfa_state_t *transitions_out)
      {
        DFATransitionsReference old_transitions = dfa_in.get_transitions(layer, old_state_index);
        for(int c = 0; c < layer_shape; ++c)
          {
            transitions_out[permutation[size_t(c)]] = old_transitions[c];
          }
      });
    }

  this->set_initial_state(dfa_in.get_initial_state());
}

void LayerPermutationDFA::validate(const dfa_shape_t& shape, const std::vector<std::vector<int>>& permutations)
{
  auto fail = [](const std::string& message)
  {
    throw std::invalid_argument("LayerPermutationDFA: " + message);
  };

  if(permutations.size() != shape.size())
    {
      fail(std::format("{:d} permutations for {:d} layers", permutations.size(), shape.size()));
    }

  for(size_t layer = 0; layer < shape.size(); ++layer)
    {
      const std::vector<int>& permutation = permutations[layer];
      int layer_shape = shape[layer];
      if(permutation.size() != size_t(layer_shape))
        {
          fail(std::format("layer {:d} has alphabet {:d} but a permutation of {:d}", layer, layer_shape, permutation.size()));
        }

      std::vector<bool> seen(size_t(layer_shape), false);
      for(int c : permutation)
        {
          if((c < 0) || (c >= layer_shape))
            {
              fail(std::format("layer {:d} maps to {:d}, outside alphabet {:d}", layer, c, layer_shape));
            }
          if(seen[size_t(c)])
            {
              fail(std::format("layer {:d} maps two characters to {:d}", layer, c));
            }
          seen[size_t(c)] = true;
        }
    }
}

std::string LayerPermutationDFA::cache_key(const dfa_shape_t& shape, const std::vector<std::vector<int>>& permutations)
{
  validate(shape, permutations);

  std::string output;
  size_t run_begin = 0;
  for(size_t layer = 1; layer <= permutations.size(); ++layer)
    {
      if((layer < permutations.size()) && (permutations[layer] == permutations[run_begin]))
        {
          continue;
        }

      if(!output.empty())
        {
          output += "_";
        }
      const std::vector<int>& permutation = permutations[run_begin];
      for(size_t c = 0; c < permutation.size(); ++c)
        {
          output += (c > 0 ? "-" : "") + std::to_string(permutation[c]);
        }
      output += "x" + std::to_string(layer - run_begin);

      run_begin = layer;
    }

  // leaves room in a 255 byte file name for the input's 64 character hash
  // and a separator.
  if(output.size() > 180)
    {
      throw std::length_error(std::format("LayerPermutationDFA: cache key of {:d} characters is too long for a file name", output.size()));
    }

  return output;
}
