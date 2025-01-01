// InverseDFA.cpp

#include "InverseDFA.h"

InverseDFA::InverseDFA(const DFA& dfa_in)
  : DFA(dfa_in.get_shape())
{
  int ndim = get_shape_size();

  dfa_in.mmap();

  if(dfa_in.get_initial_state() < 2)
    {
      // accept all or reject all input
      this->set_initial_state(1 - dfa_in.get_initial_state());
      return;
    }

  // same transitions as original DFA, but final masks are inverted.

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = dfa_in.get_layer_shape(layer);

      build_layer(layer, dfa_in.get_layer_size(layer), [&](dfa_state_t old_state_index, dfa_state_t *transitions_out)
      {
        DFATransitionsReference old_transitions = dfa_in.get_transitions(layer, old_state_index);
        for(int i = 0; i < layer_shape; ++i)
          {
	    if(old_transitions[i] < 2)
	      {
		// accept/reject case
		transitions_out[i] = 1 - old_transitions[i];
	      }
	    else
	      {
		// everything else has inverse in place
		transitions_out[i] = old_transitions[i];
	      }
          }
      });
    }

  this->set_initial_state(dfa_in.get_initial_state());
}
