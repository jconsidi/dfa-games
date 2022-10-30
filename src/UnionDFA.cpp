// UnionDFA.cpp

#include "UnionDFA.h"

static const BinaryFunction union_function([](bool l, bool r) {return l || r;});

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const DFA<ndim, shape_pack...>& left_in,
					const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, union_function)
{
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(UnionDFA);
