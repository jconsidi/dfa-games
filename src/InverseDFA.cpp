// InverseDFA.cpp

#include "InverseDFA.h"

InverseDFA::InverseDFA(const DFA& dfa_in)
{
  // same transitions as original DFA, but final masks are inverted.

  const std::vector<DFAState>& mask_transitions = dfa_in.state_transitions[63];
  for(int state = 0; state < mask_transitions.size(); ++state)
    {
      uint64_t inverted_states[DFA_MAX];
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  inverted_states[i] = !mask_transitions[state].transitions[i];
	}

      add_state(63, inverted_states);
    }

  for(int layer = 62; layer >= 0; --layer)
    {
      const std::vector<DFAState>& layer_transitions = dfa_in.state_transitions[layer];
      for(int state = 0; state < layer_transitions.size(); ++state)
	{
	  add_state(layer, layer_transitions[state].transitions);
	}
    }
}
