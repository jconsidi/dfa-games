// DFA.cpp

#include <assert.h>

#include <bit>
#include <iostream>

#include "DFA.h"

DFA::DFA()
{
}

int DFA::add_state(int layer, uint64_t next_states[DFA_MAX])
{
  if(layer == 0)
    {
      // only one initial state allowed
      if(state_transitions[0].size() > 0)
	{
	  throw std::logic_error("tried to add second initial state");
	}
    }

  // state range checks

  if(layer < 62)
    {
      // make sure next states exist
      int next_layer_size = state_counts[layer + 1].size();
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  if(next_states[i] >= next_layer_size)
	    {
	      throw std::logic_error("non-existent state");
	    }
	}
    }
  else
    {
      // last layer has bitmaps to look up last square
      int max_state = 1 << DFA_MAX;
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  if(next_states[i] >= max_state)
	    {
	      throw std::logic_error("invalid state bitmap");
	    }
	}
    }

  // add new state

  int new_state = state_counts[layer].size();
  uint64_t new_count = 0;
  for(int i = 0; i < DFA_MAX; ++i)
    {
      new_count += (layer < 62) ? state_counts[layer+1][next_states[i]] : std::popcount(next_states[i]);
      state_transitions[layer].push_back(next_states[i]);
    }
  state_counts[layer].push_back(new_count);

  assert(state_counts[layer].size() == new_state + 1);
  assert(state_transitions[layer].size() == (new_state + 1) * DFA_MAX);

  return new_state;
}

int DFA::size() const
{
  return state_counts[0][0];
}
