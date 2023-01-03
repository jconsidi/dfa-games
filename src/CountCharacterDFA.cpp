// CountCharacterDFA.cpp

// internal logical states count the number of matching pieces.

#include <iostream>
#include <vector>

#include "CountCharacterDFA.h"

CountCharacterDFA::CountCharacterDFA(const dfa_shape_t& shape_in, int c_in, int count_in)
  : CountCharacterDFA(shape_in, c_in, count_in, count_in)
{
}

CountCharacterDFA::CountCharacterDFA(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max)
  : CountCharacterDFA(shape_in, c_in, count_min, count_max, 0)
{
}

CountCharacterDFA::CountCharacterDFA(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min)
  : CountCharacterDFA(shape_in, c_in, count_min, count_max, layer_min, shape_in.size() - 1)
{
}

CountCharacterDFA::CountCharacterDFA(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min, int layer_max)
  : DedupedDFA(shape_in)
{
  int ndim = get_shape_size();

  if(!((0 <= count_min) && (count_min <= count_max) && (count_max <= ndim)))
    {
      throw std::invalid_argument("must have 0 <= count_min <= count_max <= ndim");
    }
  if(!((0 <= layer_min) && (layer_min <= layer_max) && (layer_max < ndim)))
    {
      throw std::invalid_argument("must have 0 <= layer_min <= layer_max < ndim");
    }

  std::vector<std::vector<dfa_state_t>> logical_states(ndim+1);

  // dummy layer at end
  {
    int layer = layer_max + 1;
    for(int i = 0; i <= ndim; ++i)
      {
	logical_states[layer].push_back((count_min <= i) && (i <= count_max));
      }
  }

  // internal layers where counting

  for(int layer = layer_max; layer >= layer_min; --layer)
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

  // layers before counting starts

  for(int layer = layer_min - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);

      DFATransitionsStaging next_states(layer_shape);
      for(int i = 0; i < layer_shape; ++i)
	{
	  next_states[i] = logical_states[layer+1].at(0);
	}
      logical_states[layer].push_back(this->add_state(layer, next_states));
    }

  this->set_initial_state(logical_states[0].at(0));
}
