// RewriteDFA.cpp

#include "RewriteDFA.h"

RewriteDFA::RewriteDFA(const DFA& dfa_in, std::function<void(int, uint64_t[64])> rewrite_func)
{
  uint64_t next_states[DFA_MAX];
  
  // setup reject/accept nodes to be available to rewrite_func

  {
    int layer = 62;

    // 0 = reject all
    for(int i = 0; i < DFA_MAX; ++i)
      {
	next_states[i] = DFA_MASK_REJECT_ALL;
      }
    add_state(layer, next_states);

    // 1 = accept all
    for(int i = 0; i < DFA_MAX; ++i)
      {
	next_states[i] = DFA_MASK_ACCEPT_ALL;
      }
    add_state(layer, next_states);
  }

  // internal reject all
  for(int i = 0; i < DFA_MAX; ++i)
    {
      next_states[i] = 0;
    }
  for(int layer = 61; layer >= 1; --layer)
    {
      add_state(layer, next_states);
    }

  // internal accept all
  for(int i = 0; i < DFA_MAX; ++i)
    {
      next_states[i] = 1;
    }
  for(int layer = 61; layer >= 1; --layer)
    {
      add_state(layer, next_states);
    }

  // rewrite input DFA

  uint64_t rewrite_states[DFA_MAX];

  // rewrite mask layer by splitting into separate states and reassembling
  std::vector<uint64_t> states_rewritten = {0, 1};
  {
    int layer = 63;

    for(int state_index = 0; state_index < (1 << DFA_MAX); ++state_index)
      {
	for(int i = 0; i < DFA_MAX; ++i)
	  {
	    rewrite_states[i] = (state_index & (1 << i)) != 0;
	  }
	rewrite_func(layer, rewrite_states);

	int new_index = 0;
	for(int i = 0; i < DFA_MAX; ++i)
	  {
	    if(rewrite_states[i])
	      {
		new_index |= 1 << i;
	      }
	  }
	states_rewritten.push_back(new_index);
	assert(states_rewritten.size() == state_index + 3);
      }
  }

  for(int layer = 62; layer >= 0; --layer)
    {
      std::vector<uint64_t> states_rewritten_previous = states_rewritten;
      states_rewritten.clear();
      states_rewritten.push_back(0);
      states_rewritten.push_back(1);

      for(int state_index = 0; state_index < dfa_in.state_transitions[layer].size(); ++state_index)
	{
	  for(int i = 0; i < DFA_MAX; ++i)
	    {
	      rewrite_states[i] = dfa_in.state_transitions[layer][state_index].transitions[i];
	    }
	  rewrite_func(layer, rewrite_states);

	  states_rewritten.push_back(add_state(layer, rewrite_states));
	  assert(states_rewritten.size() == state_index + 3);
	}
    }
}
