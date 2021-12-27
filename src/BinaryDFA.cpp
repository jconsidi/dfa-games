// BinaryDFA.cpp

#include "BinaryDFA.h"

BinaryDFA::BinaryDFA(const DFA& left_in, const DFA& right_in, uint64_t (*leaf_func)(uint64_t, uint64_t))
  : DFA()
{
  BinaryBuildCache cache(left_in, right_in, leaf_func);
  binary_build(0, 0, 0, cache);
}

uint64_t BinaryDFA::binary_build(int layer, uint64_t left_state, uint64_t right_state, BinaryBuildCache& cache)
{
  if(layer == 63)
    {
      return cache.leaf_func(left_state, right_state);
    }

  BinaryBuildCacheLayer& layer_cache = cache.layers[layer];
  BinaryBuildCacheLayerKey key(left_state, right_state);
  if(layer_cache.contains(key))
    {
      return layer_cache[key];
    }

  uint64_t transitions[DFA_MAX];
  for(int i = 0; i < DFA_MAX; ++i)
    {
      transitions[i] = binary_build(layer + 1,
				    cache.left.state_transitions[layer][left_state * DFA_MAX + i],
				    cache.right.state_transitions[layer][right_state * DFA_MAX + i],
				    cache);
    }

  uint64_t new_state = add_state(layer, transitions);
  layer_cache[key] = new_state;
  return new_state;
}
