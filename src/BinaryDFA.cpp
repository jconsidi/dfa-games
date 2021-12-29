// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <assert.h>

BinaryDFA::BinaryDFA(const DFA& left_in, const DFA& right_in, uint64_t (*leaf_func)(uint64_t, uint64_t))
  : DFA()
{
  BinaryBuildCache cache(left_in, right_in, leaf_func);
  binary_build(0, 0, 0, cache);
}

BinaryDFA::BinaryDFA(const std::vector<const DFA *> dfas_in, uint64_t (*leaf_func)(uint64_t, uint64_t))
{
  if(dfas_in.size() <= 0)
    {
      throw std::logic_error("zero dfas passed to BinaryDFA vector constructor");
    }

  if(dfas_in.size() == 1)
    {
      // copy singleton input
      const DFA *source = dfas_in[0];
      for(int layer = 62; layer >= 0; --layer)
	{
	  state_counts[layer] = source->state_counts[layer];
	  state_transitions[layer] = source->state_transitions[layer];

	  assert(state_transitions[layer].size() == state_counts[layer].size());
	}

      return;
    }

  if(dfas_in.size() == 2)
    {
      BinaryBuildCache cache(*(dfas_in[0]), *(dfas_in[1]), leaf_func);
      binary_build(0, 0, 0, cache);
      return;
    }

  // more than two input dfas. merge first two, then add the rest one
  // at a time. the last one will be this DFA.

  BinaryDFA *previous_dfa = new BinaryDFA(*(dfas_in[0]), *(dfas_in[1]), leaf_func);
  for(int i = 2; i < dfas_in.size() - 1; ++i)
    {
      BinaryDFA *next_dfa = new BinaryDFA(*previous_dfa, *(dfas_in[i]), leaf_func);
      delete previous_dfa;
      previous_dfa = next_dfa;
    }

  // merge penultimate result with last input DFA.
  BinaryBuildCache cache(*previous_dfa, *(dfas_in[dfas_in.size() - 1]), leaf_func);
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
				    cache.left.state_transitions[layer][left_state].transitions[i],
				    cache.right.state_transitions[layer][right_state].transitions[i],
				    cache);
    }

  uint64_t new_state = add_state(layer, transitions);
  layer_cache[key] = new_state;
  return new_state;
}
