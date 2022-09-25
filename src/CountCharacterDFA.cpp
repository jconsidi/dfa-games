// CountCharacterDFA.cpp

// internal logical states count the number of matching pieces.

#include <iostream>
#include <vector>

#include "CountCharacterDFA.h"

template<int ndim, int... shape_pack>
CountCharacterDFA<ndim, shape_pack...>::CountCharacterDFA(int c_in, int count_in)
  : CountCharacterDFA(c_in, count_in, count_in)
{
}

template<int ndim, int... shape_pack>
CountCharacterDFA<ndim, shape_pack...>::CountCharacterDFA(int c_in, int count_min, int count_max)
{
  if(!((0 <= count_min) && (count_min <= count_max) && (count_max <= ndim)))
    {
      throw std::invalid_argument("must have 0 <= count_min <= count_max <= ndim");
    }

  std::vector<dfa_state_t> logical_states[ndim+1];

  // dummy layer at end
  {
    int layer = ndim;
    for(int i = 0; i <= ndim; ++i)
      {
	logical_states[layer].push_back((count_min <= i) && (i <= count_max));
      }
  }

  // internal layers

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);

      for(int layer_count = 0; layer_count <= layer; ++layer_count)
	{
	  DFATransitionsStaging next_states(layer_shape);
	  for(int i = 0; i < layer_shape; ++i)
	    {
	      next_states[i] = logical_states[layer+1].at((i == c_in) ? (layer_count + 1) : layer_count);
	    }
	  logical_states[layer].push_back(this->add_state(layer, next_states));
	}
    }

  this->set_initial_state(logical_states[0].at(0));
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(CountCharacterDFA);
