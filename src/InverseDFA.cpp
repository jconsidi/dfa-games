// InverseDFA.cpp

#include "InverseDFA.h"

InverseDFA::InverseDFA(const DFA& dfa_in)
{
  // same transitions as original DFA, but final masks are inverted.

  const std::vector<uint64_t>& mask_transitions = dfa_in.state_transitions[62];
  for(int state_offset = 0; state_offset < mask_transitions.size(); state_offset += DFA_MAX)
    {
      uint64_t inverted_states[DFA_MAX];
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  inverted_states[i] = mask_transitions[state_offset + i] ^ DFA_MASK_ACCEPT_ALL;
	}

      add_state(62, inverted_states);
    }

  for(int layer = 61; layer >= 0; --layer)
    {
      const std::vector<uint64_t>& layer_transitions = dfa_in.state_transitions[layer];
      for(int state_offset = 0; state_offset < layer_transitions.size(); state_offset += DFA_MAX)
	{
	  uint64_t copied_states[DFA_MAX];
	  for(int i = 0; i < DFA_MAX; ++i)
	    {
	      copied_states[i] = layer_transitions[state_offset + i];
	    }
	  add_state(layer, copied_states);
	}
    }
}
