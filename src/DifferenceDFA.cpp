// DifferenceDFA.cpp

#include "DifferenceDFA.h"

static const BinaryFunction difference_function([](bool l, bool r) {return l && !r;});

template<int ndim, int... shape_pack>
DifferenceDFA<ndim, shape_pack...>::DifferenceDFA(const DFA<ndim, shape_pack...>& left_in, const DFA<ndim, shape_pack...>& right_in)
  : BinaryDFA<ndim, shape_pack...>(left_in, right_in, difference_function)
{
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(DifferenceDFA);
