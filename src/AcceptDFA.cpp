// AcceptDFA.cpp

#include "AcceptDFA.h"

template <int ndim, int... shape_pack>
AcceptDFA<ndim, shape_pack...>::AcceptDFA()
{
  this->add_uniform_states();

  // accept state at top
  int top_shape = this->get_layer_shape(0);
  DFATransitions next_states(top_shape);
  for(int i = 0; i < top_shape; ++i)
    {
      next_states[i] = 1;
    }
  this->add_state(0, next_states);
}

// template instantiations

#include "ChessDFA.h"
#include "TicTacToeGame.h"

template class AcceptDFA<CHESS_TEMPLATE_PARAMS>;
template class AcceptDFA<TICTACTOE2_DFA_PARAMS>;
template class AcceptDFA<TICTACTOE3_DFA_PARAMS>;
template class AcceptDFA<TICTACTOE4_DFA_PARAMS>;
