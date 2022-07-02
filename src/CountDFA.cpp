// count_dfa.cpp

// internal state meanings:
// logical k => k pieces in previous squares

#include <iostream>
#include <vector>

#include "CountDFA.h"

template<int ndim, int... shape_pack>
CountDFA<ndim, shape_pack...>::CountDFA(int num_pieces)
{
  // current layer states

  std::vector<dfa_state_t> current_layer_states;
  for(int i = 0; i <= ndim; ++i)
    {
      current_layer_states.push_back(i == num_pieces);
    }

  // build rest of layers backwards

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);
      std::vector<dfa_state_t> previous_layer_states = current_layer_states;
      current_layer_states.clear();

      DFATransitions next_states(layer_shape);

      for(int previous_pieces = 0; previous_pieces <= layer; ++previous_pieces)
	{
	  next_states[0] = previous_layer_states.at(previous_pieces);
	  for(int j = 1; j < layer_shape; ++j)
	    {
	      next_states[j] = previous_layer_states.at(previous_pieces + 1);
	    }
	  current_layer_states.push_back(this->add_state(layer, next_states));
	}
    }

  assert(current_layer_states.size() == 1);
  this->set_initial_state(current_layer_states[0]);
}

// template instantiations

#include "DFAParams.h"

template class CountDFA<CHESS_DFA_PARAMS>;
template class CountDFA<TEST4_DFA_PARAMS>;
template class CountDFA<TEST5_DFA_PARAMS>;
template class CountDFA<TICTACTOE2_DFA_PARAMS>;
template class CountDFA<TICTACTOE3_DFA_PARAMS>;
template class CountDFA<TICTACTOE4_DFA_PARAMS>;
