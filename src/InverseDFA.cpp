// InverseDFA.cpp

#include "InverseDFA.h"

InverseDFA::InverseDFA(const DFA& dfa_in)
{
  // same transitions as original DFA, but final masks are inverted.

  int last_layer_size = dfa_in.get_layer_size(63);
  for(int state_index = 0; state_index < last_layer_size; ++state_index)
    {
      const DFAState& old_state(dfa_in.get_state(63, state_index));

      uint64_t inverted_states[DFA_MAX];
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  inverted_states[i] = !old_state.transitions[i];
	}

      int new_state_index = add_state(63, inverted_states);
      assert(new_state_index == state_index);
    }

  for(int layer = 62; layer >= 0; --layer)
    {
      int layer_size = dfa_in.get_layer_size(layer);
      for(int state_index = 0; state_index < layer_size; ++state_index)
	{
	  const DFAState &old_state(dfa_in.get_state(layer, state_index));
	  int new_state_index = add_state(layer, old_state.transitions);
	  assert(new_state_index == state_index);
	}
    }
}
