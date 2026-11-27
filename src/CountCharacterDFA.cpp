// CountCharacterDFA.cpp

// internal logical states count the number of matching pieces.

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
  : CountCharacterDFA(shape_in, c_in, count_min, count_max, layer_min, int(shape_in.size()) - 1)
{
}

DFATable CountCharacterDFA::build(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min, int layer_max)
{
  int ndim = int(shape_in.size());

  if(!((0 <= count_min) && (count_min <= count_max) && (count_max <= ndim)))
    {
      throw std::invalid_argument("must have 0 <= count_min <= count_max <= ndim");
    }
  if(!((0 <= layer_min) && (layer_min <= layer_max) && (layer_max < ndim)))
    {
      throw std::invalid_argument("must have 0 <= layer_min <= layer_max < ndim");
    }

  DFATable table(shape_in);

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
      int layer_shape = shape_in[layer];

      for(int layer_count = 0; layer_count <= layer; ++layer_count)
	{
	  std::vector<dfa_state_t> next_states(layer_shape);
	  for(int i = 0; i < layer_shape; ++i)
	    {
	      next_states[i] = logical_states[layer+1].at((i == c_in) ? (layer_count + 1) : layer_count);
	    }
	  logical_states[layer].push_back(table.add_state(layer, next_states));
	}
    }

  // layers before counting starts

  for(int layer = layer_min - 1; layer >= 0; --layer)
    {
      int layer_shape = shape_in[layer];

      std::vector<dfa_state_t> next_states(layer_shape);
      for(int i = 0; i < layer_shape; ++i)
	{
	  next_states[i] = logical_states[layer+1].at(0);
	}
      logical_states[layer].push_back(table.add_state(layer, next_states));
    }

  table.set_initial_state(logical_states[0].at(0));
  return table;
}

CountCharacterDFA::CountCharacterDFA(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min, int layer_max)
  : TableDFA(build(shape_in, c_in, count_min, count_max, layer_min, layer_max))
{
}
