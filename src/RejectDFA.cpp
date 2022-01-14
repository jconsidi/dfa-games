// RejectDFA.cpp

#include "RejectDFA.h"

template <int ndim, int... shape_pack>
RejectDFA<ndim, shape_pack...>::RejectDFA()
{
  this->add_uniform_states();

  // reject state at top
  this->add_state(0, [](int i) {return 0;});
}

// template instantiations

#include "ChessDFA.h"
#include "TicTacToeGame.h"

template class RejectDFA<CHESS_TEMPLATE_PARAMS>;
template class RejectDFA<TICTACTOE2_DFA_PARAMS>;
template class RejectDFA<TICTACTOE3_DFA_PARAMS>;
template class RejectDFA<TICTACTOE4_DFA_PARAMS>;
