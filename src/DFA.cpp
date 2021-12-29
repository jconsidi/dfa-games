// DFA.cpp

#include <assert.h>

#include <bit>
#include <iostream>

#include "DFA.h"

DFA::DFA()
  : state_lookup(new DFAStateMap[63])
{
  for(int layer = 0; layer < 63; ++layer)
    {

    }
}

DFA::~DFA()
{
  if(state_lookup)
    {
      delete[] state_lookup;
      state_lookup = 0;
    }
}

int DFA::add_state(int layer, const uint64_t next_states[DFA_MAX])
{
  assert(state_lookup);

  if(layer == 0)
    {
      // only one initial state allowed
      if(state_transitions[0].size() > 0)
	{
	  throw std::logic_error("tried to add second initial state");
	}

      delete[] state_lookup;
      state_lookup = 0;
    }

  // state range checks

  if(layer < 62)
    {
      // make sure next states exist
      int next_layer_size = state_counts[layer + 1].size();
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  assert(next_states[i] < next_layer_size);
	}
    }
  else
    {
      // last layer has bitmaps to look up last square
      int max_state = 1 << DFA_MAX;
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  assert(next_states[i] < max_state);
	}
    }

  // check if state already exists

  DFAState state_key(next_states);

  if(layer > 0)
    {
      if(state_lookup[layer].count(state_key))
	{
	  return state_lookup[layer][state_key];
	}
    }

  // add new state

  int new_state = state_counts[layer].size();
  uint64_t new_count = 0;
  for(int i = 0; i < DFA_MAX; ++i)
    {
      new_count += (layer < 62) ? state_counts[layer+1][next_states[i]] : std::popcount(next_states[i]);
    }
  state_counts[layer].push_back(new_count);
  state_transitions[layer].emplace_back(next_states);

  assert(state_counts[layer].size() == new_state + 1);
  assert(state_transitions[layer].size() == (new_state + 1));

  if(layer > 0)
    {
      state_lookup[layer][state_key] = new_state;
    }

  return new_state;
}

void DFA::debug_counts(std::string debug_name) const
{
  for(int layer = 0; layer < 63; ++layer)
    {
      std::cerr << debug_name << " counts: layer " << layer << ": " << state_counts[layer][0];
      for(int state = 1; state < state_counts[layer].size(); ++state)
	{
	  std::cerr << ", " << state_counts[layer][state];
	}
      std::cerr << std::endl;
    }
  std::cerr << std::endl;

  std::cerr.flush();
}

bool DFA::ready() const
{
  return state_counts[0].size() != 0;
}

DFA::size_type DFA::size() const
{
  return state_counts[0][0];
}

DFA::size_type DFA::states() const
{
  DFA::size_type states_out = 0;

  for(int layer = 0; layer < 63; ++layer)
    {
      states_out += state_counts[layer].size();
    }

  // ignoring states implied by masks in last layer

  return states_out;
}
