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
  BinaryBuildCache<ndim, shape_pack...> cache(left_in, right_in, leaf_func);
  binary_build(0, 0, 0, cache);
}

template <int ndim, int... shape_pack>
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>& dfas_in,
					  uint64_t (*leaf_func)(uint64_t, uint64_t))
  : DFA<ndim, shape_pack...>()
{
  // confirm commutativity
  assert(leaf_func(0, 1) == leaf_func(1, 0));

  // special case 0-2 input DFAs
  switch(dfas_in.size())
    {
    case 0:
      {
	throw std::logic_error("zero dfas passed to BinaryDFA vector constructor");
      }

    case 1:
      {
	// copy singleton input
	std::shared_ptr<const DFA<ndim, shape_pack...>> source = dfas_in[0];
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
    }

  // three or more DFAs. will greedily merge the two smallest DFAs
  // counting states until two DFAs left, then merge the last two for
  // this DFA.

  std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>> dfas_temp;
  for(int i = 0; i < dfas_in.size(); ++i)
    {
      dfas_temp.push_back(dfas_in[i]);
    }

  struct
  {
    bool operator()(std::shared_ptr<const DFA<ndim, shape_pack...>> a,
		    std::shared_ptr<const DFA<ndim, shape_pack...>> b) const
    {
      return a->states() > b->states();
    }
  } reverse_less;

  while(dfas_temp.size() > 2)
    {
      int temp_size = dfas_temp.size();

      // partial sort so last two DFAs have the least positions
      std::nth_element(dfas_temp.begin(),
		       dfas_temp.begin() + (temp_size - 2),
		       dfas_temp.end(),
		       reverse_less);

      std::shared_ptr<const DFA<ndim, shape_pack...>>& second_last = dfas_temp[temp_size - 2];
      std::shared_ptr<const DFA<ndim, shape_pack...>> last = dfas_temp[temp_size - 1];

      if(second_last->states() >= 1024)
	{
	  std::cerr << "  merging DFAs with " << last->states() << " states and " << second_last->states() << " states" << std::endl;
	}
      second_last = std::shared_ptr<const DFA<ndim, shape_pack...>>(new BinaryDFA(*second_last, *last, leaf_func));
      dfas_temp.pop_back();

      assert(dfas_temp.size() == temp_size - 1);
    }

  assert(dfas_temp.size() == 2);

  if((dfas_temp[0]->states() >= 1024) || (dfas_temp[1]->states() >= 1024))
    {
      std::cerr << "  merging DFAs with " << dfas_temp[0]->states() << " states and " << dfas_temp[1]->states() << " states" << std::endl;
    }

  BinaryBuildCache cache(*(dfas_temp[0]), *(dfas_temp[1]), leaf_func);
  binary_build(0, 0, 0, cache);
  assert(this->ready());

  if(this->states() >= 1024)
    {
      std::cerr << " merged DFA has " << this->states() << " and " << this->size() << " positions" << std::endl;
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

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class BinaryDFA<CHESS_DFA_PARAMS>;
template class BinaryDFA<TEST_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE2_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE3_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE4_DFA_PARAMS>;
