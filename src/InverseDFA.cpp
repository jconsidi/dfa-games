// InverseDFA.cpp

#include "InverseDFA.h"

template<int ndim, int... shape_pack>
InverseDFA<ndim, shape_pack...>::InverseDFA(const DFA<ndim, shape_pack...>& dfa_in)
{
  if(dfa_in.get_initial_state() < 2)
    {
      // accept all or reject all input
      this->set_initial_state(1 - dfa_in.get_initial_state());
      return;
    }

  // same transitions as original DFA, but final masks are inverted.

  int last_layer_size = dfa_in.get_layer_size(ndim - 1);
  for(int old_state_index = 2; old_state_index < last_layer_size; ++old_state_index)
    {
      const DFATransitions& old_transitions = dfa_in.get_transitions(ndim - 1, old_state_index);
      int new_state_index = this->add_state(ndim - 1, [old_transitions](int i){return !old_transitions[i];});
      assert(new_state_index == old_state_index);
    }

  for(int layer = ndim - 2; layer >= 0; --layer)
    {
      int layer_size = dfa_in.get_layer_size(layer);

      for(int old_state_index = 2; old_state_index < layer_size; ++old_state_index)
	{
	  const DFATransitions &old_transitions = dfa_in.get_transitions(layer, old_state_index);
	  int new_state_index = this->add_state(layer, [&](int i)
	  {
	    if(old_transitions[i] < 2)
	      {
		// accept/reject case
		return 1 - old_transitions[i];
	      }
	    else
	      {
		// everything else has inverse in place
		return old_transitions[i];
	      }
	  });
	  assert(new_state_index == old_state_index);
	}
    }

  this->set_initial_state(dfa_in.get_initial_state());
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class InverseDFA<CHESS_DFA_PARAMS>;
template class InverseDFA<TEST_DFA_PARAMS>;
template class InverseDFA<TICTACTOE2_DFA_PARAMS>;
template class InverseDFA<TICTACTOE3_DFA_PARAMS>;
template class InverseDFA<TICTACTOE4_DFA_PARAMS>;
