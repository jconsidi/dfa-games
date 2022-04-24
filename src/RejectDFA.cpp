// RejectDFA.cpp

#include "RejectDFA.h"

template <int ndim, int... shape_pack>
RejectDFA<ndim, shape_pack...>::RejectDFA()
{
  this->set_initial_state(0);
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class RejectDFA<CHESS_DFA_PARAMS>;
template class RejectDFA<TEST_DFA_PARAMS>;
template class RejectDFA<TICTACTOE2_DFA_PARAMS>;
template class RejectDFA<TICTACTOE3_DFA_PARAMS>;
template class RejectDFA<TICTACTOE4_DFA_PARAMS>;
