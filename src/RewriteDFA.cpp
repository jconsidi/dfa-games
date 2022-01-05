// RewriteDFA.cpp

#include "RewriteDFA.h"

RewriteDFA::RewriteDFA(const DFA& dfa_in, std::function<void(int, uint64_t[64])> rewrite_func)
{
  // setup reject/accept nodes to be available to rewrite_func

  add_uniform_states();

  // rewrite input DFA

  uint64_t rewrite_states[DFA_MAX];

  // rewrite mask layer by splitting into separate states and reassembling
  std::vector<uint64_t> states_rewritten = {0, 1};
  for(int layer = 63; layer >= 0; --layer)
    {
      std::vector<uint64_t> states_rewritten_previous = states_rewritten;
      states_rewritten.clear();
      states_rewritten.push_back(0);
      states_rewritten.push_back(1);

      int layer_size = dfa_in.get_layer_size(layer);
      for(int state_index = 0; state_index < layer_size; ++state_index)
	{
	  const DFAState& old_state(dfa_in.get_state(layer, state_index));
	  for(int i = 0; i < DFA_MAX; ++i)
	    {
	      rewrite_states[i] = old_state.transitions[i];
	    }
	  rewrite_func(layer, rewrite_states);

	  states_rewritten.push_back(add_state(layer, rewrite_states));
	  assert(states_rewritten.size() == state_index + 3);
	}
    }
}
