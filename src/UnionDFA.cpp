// UnionDFA.cpp

#include "UnionDFA.h"

const BinaryFunction union_function([](bool l, bool r) {return l || r;});

UnionDFA::UnionDFA(const DFA& left_in,
		   const DFA& right_in)
  : BinaryDFA(left_in, right_in, union_function)
{
}
