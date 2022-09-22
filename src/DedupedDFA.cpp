// DedupedDFA.cpp

#include "DedupedDFA.h"

#include <iostream>

template<int ndim, int... shape_pack>
DedupedDFA<ndim, shape_pack...>::DedupedDFA()
  : DFA<ndim, shape_pack...>(),
    state_lookup(new DFATransitionsMap[ndim])
{
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
dfa_state_t DedupedDFA<ndim, shape_pack...>::add_state(int layer, const DFATransitionsStaging& next_states)
{
  assert(state_lookup);
  assert((0 <= layer) && (layer < ndim));
  assert(next_states.size() == this->get_layer_shape(layer));

  // check if we already added this state

  auto search = this->state_lookup[layer].find(next_states);
  if(search != this->state_lookup[layer].end())
    {
      return search->second;
    }

  // add new state

  dfa_state_t new_state_id = DFA<ndim, shape_pack...>::add_state(layer, next_states);
  this->state_lookup[layer][next_states] = new_state_id;

  return new_state_id;
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
