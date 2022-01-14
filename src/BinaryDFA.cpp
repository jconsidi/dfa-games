// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <assert.h>

#include <iostream>

template <int ndim, int... shape_pack>
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const DFA<ndim, shape_pack...>& left_in,
					  const DFA<ndim, shape_pack...>& right_in,
					  uint64_t (*leaf_func)(uint64_t, uint64_t))
  : DFA<ndim, shape_pack...>()
{
  // TODO: sanity check left/right shape match

  BinaryBuildCache<ndim, shape_pack...> cache(left_in, right_in, leaf_func);
  binary_build(0, 0, 0, cache);
}

template <int ndim, int... shape_pack>
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const std::vector<const DFA<ndim, shape_pack...> *> dfas_in,
					  uint64_t (*leaf_func)(uint64_t, uint64_t))
  : DFA<ndim, shape_pack...>()
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
	const DFA<ndim, shape_pack...> *source = dfas_in[0];
	for(int layer = ndim - 1; layer >= 0; --layer)
	  {
	    int layer_size = source->get_layer_size(layer);
	    for(int state_index = 0; state_index < layer_size; ++state_index)
	      {
		const DFATransitions& transitions(source->get_transitions(layer, state_index));
		int new_state_index = this->add_state(layer, transitions);
		assert(new_state_index == state_index);
	      }
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

  if(this->states() >= 1 << 10)
    {
      std::cerr << "  final merged has " << this->states() << " states" << std::endl;
    }
}

template <int ndim, int... shape_pack>
uint64_t BinaryDFA<ndim, shape_pack...>::binary_build(int layer,
						      uint64_t left_index,
						      uint64_t right_index,
						      BinaryBuildCache<ndim, shape_pack...>& cache)
{
  if(layer == ndim)
    {
      return cache.leaf_func(left_index, right_index);
    }

  BinaryBuildCacheLayer& layer_cache = cache.layers[layer];
  BinaryBuildCacheLayerKey key(left_index, right_index);
  if(layer_cache.contains(key))
    {
      return layer_cache[key];
    }

  const DFATransitions& left_transitions(cache.left.get_transitions(layer, left_index));
  const DFATransitions& right_transitions(cache.right.get_transitions(layer, right_index));

  int layer_shape = this->get_layer_shape(layer);
  DFATransitions transitions(layer_shape);
  for(int i = 0; i < layer_shape; ++i)
    {
      transitions[i] = binary_build(layer + 1,
				    left_transitions[i],
				    right_transitions[i],
				    cache);
    }

  uint64_t new_state = this->add_state(layer, transitions);
  layer_cache[key] = new_state;
  return new_state;
}

// template instantiations

#include "ChessDFA.h"
#include "TicTacToeGame.h"

template class BinaryDFA<CHESS_TEMPLATE_PARAMS>;
template class BinaryDFA<TICTACTOE2_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE3_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE4_DFA_PARAMS>;
