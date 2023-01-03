// DifferenceDFA.cpp

#include "DifferenceDFA.h"

static const BinaryFunction difference_function([](bool l, bool r) {return l && !r;});

DifferenceDFA::DifferenceDFA(const DFA& left_in, const DFA& right_in)
  : BinaryDFA(left_in, right_in, difference_function)
{
}
