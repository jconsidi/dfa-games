// LayerPermutationDFA.cpp

#include "LayerPermutationDFA.h"

#include <format>
#include <stdexcept>

std::vector<std::vector<int>> LayerPermutationDFA::sources_for(const dfa_shape_t& shape, const std::vector<std::vector<int>>& permutations)
{
  validate(shape, permutations);

  // character permutations[layer][c] takes the input's transition on c
  std::vector<std::vector<int>> sources;
  for(const std::vector<int>& permutation : permutations)
    {
      std::vector<int> layer_sources(permutation.size());
      for(size_t c = 0; c < permutation.size(); ++c)
	{
	  layer_sources[size_t(permutation[c])] = int(c);
	}
      sources.push_back(layer_sources);
    }
  return sources;
}

LayerPermutationDFA::LayerPermutationDFA(shared_dfa_ptr dfa_in, const std::vector<std::vector<int>>& permutations)
  : RelabelDFA(dfa_in, sources_for(dfa_in->get_shape(), permutations))
{
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
