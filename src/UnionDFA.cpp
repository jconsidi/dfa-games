// UnionDFA.cpp

#include "UnionDFA.h"

template<int ndim, int... shape_pack>
dfa_state_t UnionDFA<ndim, shape_pack...>::union_mask(dfa_state_t left_mask, dfa_state_t right_mask)
{
  return left_mask | right_mask;
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const DFA<ndim, shape_pack...>& left_in,
						      const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, union_mask)
{
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(UnionDFA);
