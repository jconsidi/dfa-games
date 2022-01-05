// DFA.cpp

#include <assert.h>

#include <iostream>

#include "DFA.h"

DFA::DFA()
  : state_lookup(new DFAStateMap[64])
{
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
  assert(layer < 64);

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

  if(layer < 63)
    {
      // make sure next states exist
      int next_layer_size = state_transitions[layer + 1].size();
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  assert(next_states[i] < next_layer_size);
	}
    }
  else
    {
      // last layer is all zero/one for reject/accept.
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  assert(next_states[i] < 2);
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

  int new_state = state_transitions[layer].size();
  state_transitions[layer].emplace_back(next_states);

  assert(state_transitions[layer].size() == (new_state + 1));

  if(layer > 0)
    {
      state_lookup[layer][state_key] = new_state;
    }

  return new_state;
}

void DFA::add_uniform_states()
{
  for(int layer = 63; layer >= 1; --layer)
    {
      uint64_t next_states[DFA_MAX] = {0};
      uint64_t reject_state = add_state(layer, next_states);
      assert(reject_state == 0);

      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = 1;
	}
      uint64_t accept_state = add_state(layer, next_states);
      assert(accept_state == 1);
    }
}

int DFA::get_layer_size(int layer) const
{
  return state_transitions[layer].size();
}

const DFAState& DFA::get_state(int layer, int state_index) const
{
  return state_transitions[layer].at(state_index);
}

bool DFA::ready() const
{
  return state_transitions[0].size() != 0;
}

DFA::size_type DFA::size() const
{
  assert(state_transitions[0].size() == 1);

  std::vector<uint64_t> previous_counts({0, 1}); // reject, accept
  for(int layer = 63; layer >= 0; --layer)
    {
      std::vector<uint64_t> current_counts;
      for(int state_index = 0; state_index < state_transitions[layer].size(); ++state_index)
	{
	  const DFAState& state(state_transitions[layer][state_index]);

	  uint64_t state_count = 0;
	  for(int i = 0; i < DFA_MAX; ++i)
	    {
	      state_count += previous_counts.at(state.transitions[i]);
	    }
	  current_counts.push_back(state_count);
	}

      previous_counts = current_counts;
    }

  assert(previous_counts.size() == 1);

  return previous_counts[0];
}

DFA::size_type DFA::states() const
{
  DFA::size_type states_out = 0;

  for(int layer = 0; layer < 64; ++layer)
    {
      states_out += state_transitions[layer].size();
    }

  // ignoring states implied by masks in last layer

  return states_out;
}
