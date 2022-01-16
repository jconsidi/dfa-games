// UnionDFA.cpp

#include "UnionDFA.h"

template<int ndim, int... shape_pack>
uint64_t UnionDFA<ndim, shape_pack...>::union_mask(uint64_t left_mask, uint64_t right_mask)
{
  return left_mask | right_mask;
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const DFA<ndim, shape_pack...>& left_in,
						      const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, union_mask)
{
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const std::vector<const DFA<ndim, shape_pack...> *>& dfas_in)
  : BinaryDFA<ndim, shape_pack...>(dfas_in, union_mask)
{
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>& dfas_in)
  : BinaryDFA<ndim, shape_pack...>(dfas_in, union_mask)
{
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class UnionDFA<CHESS_DFA_PARAMS>;
template class UnionDFA<TEST_DFA_PARAMS>;
template class UnionDFA<TICTACTOE2_DFA_PARAMS>;
template class UnionDFA<TICTACTOE3_DFA_PARAMS>;
template class UnionDFA<TICTACTOE4_DFA_PARAMS>;
