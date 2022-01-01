// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <assert.h>

#include <iostream>

BinaryDFA::BinaryDFA(const DFA& left_in, const DFA& right_in, uint64_t (*leaf_func)(uint64_t, uint64_t))
  : DFA()
{
  BinaryBuildCache cache(left_in, right_in, leaf_func);
  binary_build(0, 0, 0, cache);
}

BinaryDFA::BinaryDFA(const std::vector<const DFA *> dfas_in, uint64_t (*leaf_func)(uint64_t, uint64_t))
{
  switch(dfas_in.size())
    {
    case 0:
      {
	throw std::logic_error("zero dfas passed to BinaryDFA vector constructor");
      }

    case 1:
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

    case 2:
      {
	// simple binary merge
	BinaryBuildCache cache(*(dfas_in[0]), *(dfas_in[1]), leaf_func);
	binary_build(0, 0, 0, cache);
	return;
      }

    case 3:
      {
	// two binary merges
	BinaryDFA temp(*(dfas_in[0]), *(dfas_in[1]), leaf_func);

	BinaryBuildCache cache(temp, *(dfas_in[2]), leaf_func);
	binary_build(0, 0, 0, cache);
	return;
      }
    }

  // four or more DFAs. will merge and rollover into a temp
  // vector. then keep popping off two DFAs from the front and merging
  // onto the end. last merge becomes this DFA.

  std::vector<BinaryDFA> dfas_temp;
  for(int i = 0; i < dfas_in.size() - 1; i += 2)
    {
      dfas_temp.emplace_back(*(dfas_in[i]),*(dfas_in[i + 1]), leaf_func);
      std::cerr << "  early merge " << (i / 2) << "/" << (dfas_in.size() / 2) << " had " << dfas_temp[dfas_temp.size() - 1].states() << " states" << std::endl;
    }
  int next_merge = 0;
  if(dfas_in.size() % 2)
    {
      dfas_temp.emplace_back(*(dfas_in[dfas_in.size() - 1]), dfas_temp[0], leaf_func);
      next_merge = 1;
      // LATER: drop temp DFA internals
      std::cerr << "  odd merge had " << dfas_temp[dfas_temp.size() - 1].states() << " states" << std::endl;
    }

  std::cerr << "  " << (dfas_temp.size() - next_merge - 1) << " merges remaining" << std::endl;
  while(next_merge + 2 < dfas_temp.size())
    {
      dfas_temp.emplace_back(dfas_temp[next_merge], dfas_temp[next_merge + 1], leaf_func);
      next_merge += 2;
      // LATER: drop temp DFA internals
      if(dfas_temp[dfas_temp.size() - 1].states() >= 1024)
	{
	  std::cerr << "  " << (dfas_temp.size() - next_merge - 1) << " merges remaining, last merge had " << dfas_temp[dfas_temp.size() - 1].states() << " states" << std::endl;
	}
    }

  assert(dfas_temp.size() - next_merge == 2);

  // merge last two into the final DFA
  BinaryBuildCache cache(dfas_temp[next_merge], dfas_temp[next_merge + 1], leaf_func);
  binary_build(0, 0, 0, cache);

  if(states() >= 1 << 10)
    {
      std::cerr << "  final merged has " << states() << " states" << std::endl;
    }
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
