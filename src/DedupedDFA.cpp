// DedupedDFA.cpp

#include "DedupedDFA.h"

#include <iostream>

template<int ndim, int... shape_pack>
DedupedDFA<ndim, shape_pack...>::DedupedDFA()
  : DFA<ndim, shape_pack...>(),
    state_lookup(new DFATransitionsMap[ndim])
{
  // add uniform states
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      dfa_state_t reject_state = add_state(layer, [](int i){return 0;});
      assert(reject_state == 0);

      dfa_state_t accept_state = add_state(layer, [](int i){return 1;});
      assert(accept_state == 1);
    }
}

template<int ndim, int... shape_pack>
DedupedDFA<ndim, shape_pack...>::~DedupedDFA()
{
  if(this->state_lookup)
    {
      delete[] this->state_lookup;
      this->state_lookup = 0;
    }
}

template<int ndim, int... shape_pack>
dfa_state_t DedupedDFA<ndim, shape_pack...>::add_state(int layer, const DFATransitions& next_states)
{
  assert(state_lookup);
  assert((0 <= layer) && (layer < ndim));
  assert(next_states.size() == this->get_layer_shape(layer));

  // state range checks

  size_t layer_shape = this->get_layer_shape(layer);

  if(layer < ndim - 1)
    {
      // make sure next states exist
      size_t next_layer_size = this->get_layer_size(layer + 1);
      for(int i = 0; i < layer_shape; ++i)
	{
	  assert(next_states[i] < next_layer_size);
	}
    }
  else
    {
      // last layer is all zero/one for reject/accept.
      for(int i = 0; i < layer_shape; ++i)
	{
	  assert(next_states[i] < 2);
	}
    }

  auto search = this->state_lookup[layer].find(next_states);
  if(search != this->state_lookup[layer].end())
    {
      return search->second;
    }

  // add new state

  size_t new_state_id = this->state_transitions[layer].size();
  this->state_transitions[layer].emplace_back(next_states);

  assert(this->state_transitions[layer].size() == (new_state_id + 1));

  this->state_lookup[layer][next_states] = new_state_id;

  return new_state_id;
}

template<int ndim, int... shape_pack>
dfa_state_t DedupedDFA<ndim, shape_pack...>::add_state(int layer, std::function<dfa_state_t(int)> transition_func)
{
  int layer_shape = this->get_layer_shape(layer);

  DFATransitions transitions(layer_shape);
  for(int i = 0; i < layer_shape; ++i)
    {
      transitions[i] = transition_func(i);
    }

  return add_state(layer, transitions);
}

template<int ndim, int... shape_pack>
void DedupedDFA<ndim, shape_pack...>::set_initial_state(dfa_state_t initial_state_in)
{
  DFA<ndim, shape_pack...>::set_initial_state(initial_state_in);

  // block further state creation
  delete[] state_lookup;
  state_lookup = 0;
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(DedupedDFA);
