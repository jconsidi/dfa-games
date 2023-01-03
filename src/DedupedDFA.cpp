// DedupedDFA.cpp

#include "DedupedDFA.h"

#include <iostream>

DedupedDFA::DedupedDFA(const dfa_shape_t& shape_in)
  : DFA(shape_in),
    state_lookup(new DFATransitionsMap[this->get_shape_size()])
{
}

DedupedDFA::~DedupedDFA()
{
  if(this->state_lookup)
    {
      delete[] this->state_lookup;
      this->state_lookup = 0;
    }
}

dfa_state_t DedupedDFA::add_state(int layer, const DFATransitionsStaging& next_states)
{
  assert(state_lookup);
  assert((0 <= layer) && (layer < get_shape_size()));
  assert(next_states.size() == this->get_layer_shape(layer));

  // check if we already added this state

  auto search = this->state_lookup[layer].find(next_states);
  if(search != this->state_lookup[layer].end())
    {
      return search->second;
    }

  // add new state

  dfa_state_t new_state_id = DFA::add_state(layer, next_states);
  this->state_lookup[layer][next_states] = new_state_id;

  return new_state_id;
}

void DedupedDFA::set_initial_state(dfa_state_t initial_state_in)
{
  DFA::set_initial_state(initial_state_in);

  // block further state creation
  delete[] state_lookup;
  state_lookup = 0;
}
