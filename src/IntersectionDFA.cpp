// IntersectionDFA.cpp

#include "IntersectionDFA.h"

static const BinaryFunction intersection_function([](bool l, bool r) {return l && r;});

IntersectionDFA::IntersectionDFA(const DFA& left_in,
				 const DFA& right_in)
  : BinaryDFA(left_in, right_in, intersection_function)
{
}
