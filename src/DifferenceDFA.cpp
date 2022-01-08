// DifferenceDFA.cpp

#include "DifferenceDFA.h"

template<int ndim, int... shape_pack>
uint64_t DifferenceDFA<ndim, shape_pack...>::difference_mask(uint64_t left_mask, uint64_t right_mask)
{
  return left_mask & ~right_mask;
}

template<int ndim, int... shape_pack>
DifferenceDFA<ndim, shape_pack...>::DifferenceDFA(const DFA<ndim, shape_pack...>& left_in, const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, difference_mask)
{
}

// template instantiations

#include "ChessDFA.h"

template class DifferenceDFA<CHESS_TEMPLATE_PARAMS>;
