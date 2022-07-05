// ExplicitDFA.cpp

#include "ExplicitDFA.h"

template<int ndim, int... shape_pack>
ExplicitDFA<ndim, shape_pack...>::ExplicitDFA()
{
  // add uniform states
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      // reject state
      set_state(layer, 0, [](int i){return 0;});

      // accept state
      set_state(layer, 1, [](int i){return 1;});
    }
}

template<int ndim, int... shape_pack>
void ExplicitDFA<ndim, shape_pack...>::set_state(int layer, dfa_state_t new_state_id, const DFATransitions& next_states)
{
  // check that this is the next state to be assigned
  assert(this->get_layer_size(layer) == new_state_id);

  // sanity check transitions
  dfa_state_t next_layer_size = this->get_layer_size(layer + 1);
  int layer_shape = this->get_layer_shape(layer);
  for(int i = 0; i < layer_shape; ++i)
    {
      assert(next_states[i] < next_layer_size);
    }

  // add the state
  this->state_transitions[layer].emplace_back(next_states);
}

template<int ndim, int... shape_pack>
void ExplicitDFA<ndim, shape_pack...>::set_state(int layer, dfa_state_t new_state_id, std::function<dfa_state_t(int)> transition_func)
{
  int layer_shape = this->get_layer_shape(layer);

  DFATransitions transitions(layer_shape);
  for(int i = 0; i < layer_shape; ++i)
    {
      transitions[i] = transition_func(i);
    }

  return set_state(layer, new_state_id, transitions);
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(ExplicitDFA);
