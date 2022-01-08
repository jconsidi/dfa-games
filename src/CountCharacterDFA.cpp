// CountCharacterDFA.cpp

// internal logical states count the number of matching pieces.

#include <iostream>
#include <vector>

#include "CountCharacterDFA.h"

template<int ndim, int... shape_pack>
CountCharacterDFA<ndim, shape_pack...>::CountCharacterDFA(int c_in, int count_in)
{
  if((count_in < 0) || ndim < count_in)
    {
      throw std::invalid_argument("count_in must be between 0 and ndim (inclusive)");
    }

  std::vector<uint64_t> logical_states[ndim+1];

  // dummy layer at end
  {
    int layer = ndim;
    for(int i = 0; i <= ndim; ++i)
      {
	logical_states[layer].push_back(i == count_in);
      }
  }

  // internal layers

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);

      for(int layer_count = 0; layer_count <= layer; ++layer_count)
	{
	  DFATransitions next_states(layer_shape);
	  for(int i = 0; i < layer_shape; ++i)
	    {
	      next_states[i] = logical_states[layer+1].at((i == c_in) ? (layer_count + 1) : layer_count);
	    }
	  logical_states[layer].push_back(this->add_state(layer, next_states));
	}
    }
}

// template instantiations

#include "ChessDFA.h"

template class CountCharacterDFA<CHESS_TEMPLATE_PARAMS>;
