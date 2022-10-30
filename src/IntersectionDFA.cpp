// IntersectionDFA.cpp

#include "IntersectionDFA.h"

static const BinaryFunction intersection_function([](bool l, bool r) {return l && r;});

template<int ndim, int... shape_pack>
IntersectionDFA<ndim, shape_pack...>::IntersectionDFA(const DFA<ndim, shape_pack...>& left_in,
						      const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, intersection_function)
{
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(IntersectionDFA);
