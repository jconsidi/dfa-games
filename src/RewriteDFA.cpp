// RewriteDFA.cpp

#include "RewriteDFA.h"

template<int ndim, int... shape_pack>
RewriteDFA<ndim, shape_pack...>::RewriteDFA(const DFA<ndim, shape_pack...>& dfa_in, std::function<void(int, DFATransitions&)> rewrite_func)
{
  // setup reject/accept nodes to be available to rewrite_func

  this->add_uniform_states();

  // rewrite input DFA

  // rewrite mask layer by splitting into separate states and reassembling
  std::vector<uint64_t> states_rewritten = {0, 1};
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      std::vector<uint64_t> states_rewritten_previous = states_rewritten;
      states_rewritten.clear();
      states_rewritten.push_back(0);
      states_rewritten.push_back(1);

      int layer_size = dfa_in.get_layer_size(layer);
      for(int state_index = 0; state_index < layer_size; ++state_index)
	{
	  const DFATransitions& old_transitions = dfa_in.get_transitions(layer, state_index);
	  DFATransitions new_transitions = old_transitions;
	  rewrite_func(layer, new_transitions);

	  states_rewritten.push_back(this->add_state(layer, new_transitions));
	  assert(states_rewritten.size() == state_index + 3);
	}
    }
}

// template instantiations

#include "ChessDFA.h"

template class RewriteDFA<CHESS_TEMPLATE_PARAMS>;
