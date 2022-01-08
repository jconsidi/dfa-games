// IntersectionDFA.cpp

#include "IntersectionDFA.h"

template<int ndim, int... shape_pack>
uint64_t IntersectionDFA<ndim, shape_pack...>::intersection_mask(uint64_t left_mask, uint64_t right_mask)
{
  return left_mask & right_mask;
}

template<int ndim, int... shape_pack>
IntersectionDFA<ndim, shape_pack...>::IntersectionDFA(const DFA<ndim, shape_pack...>& left_in,
						      const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, intersection_mask)
{
}

template<int ndim, int... shape_pack>
IntersectionDFA<ndim, shape_pack...>::IntersectionDFA(const std::vector<const DFA<ndim, shape_pack...> *> dfas_in)
  : BinaryDFA<ndim, shape_pack...>(dfas_in, intersection_mask)
{
}

// template instantiations

#include "ChessDFA.h"

template class IntersectionDFA<CHESS_TEMPLATE_PARAMS>;
