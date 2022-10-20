// IntersectionDFA.cpp

#include "IntersectionDFA.h"

template<int ndim, int... shape_pack>
dfa_state_t IntersectionDFA<ndim, shape_pack...>::intersection_mask(dfa_state_t left_mask, dfa_state_t right_mask)
{
  return left_mask & right_mask;
}

template<int ndim, int... shape_pack>
IntersectionDFA<ndim, shape_pack...>::IntersectionDFA(const DFA<ndim, shape_pack...>& left_in,
						      const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, intersection_mask)
{
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(IntersectionDFA);
