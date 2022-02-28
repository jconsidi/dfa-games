// DifferenceDFA.cpp

#include "DifferenceDFA.h"

template<int ndim, int... shape_pack>
dfa_state_t DifferenceDFA<ndim, shape_pack...>::difference_mask(dfa_state_t left_mask, dfa_state_t right_mask)
{
  return left_mask & ~right_mask;
}

template<int ndim, int... shape_pack>
DifferenceDFA<ndim, shape_pack...>::DifferenceDFA(const DFA<ndim, shape_pack...>& left_in, const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, difference_mask)
{
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class DifferenceDFA<CHESS_DFA_PARAMS>;
template class DifferenceDFA<TICTACTOE2_DFA_PARAMS>;
template class DifferenceDFA<TICTACTOE3_DFA_PARAMS>;
template class DifferenceDFA<TICTACTOE4_DFA_PARAMS>;
