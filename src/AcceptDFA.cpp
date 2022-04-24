// AcceptDFA.cpp

#include "AcceptDFA.h"

template <int ndim, int... shape_pack>
AcceptDFA<ndim, shape_pack...>::AcceptDFA()
{
  this->set_initial_state(1);
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class AcceptDFA<CHESS_DFA_PARAMS>;
template class AcceptDFA<TEST_DFA_PARAMS>;
template class AcceptDFA<TICTACTOE2_DFA_PARAMS>;
template class AcceptDFA<TICTACTOE3_DFA_PARAMS>;
template class AcceptDFA<TICTACTOE4_DFA_PARAMS>;
