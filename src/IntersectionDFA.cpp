// IntersectionDFA.cpp

#include "IntersectionDFA.h"

static uint64_t intersection_mask(uint64_t left_mask, uint64_t right_mask)
{
  return left_mask & right_mask;
}

IntersectionDFA::IntersectionDFA(const DFA& left_in, const DFA& right_in)
  : BinaryDFA(left_in, right_in, intersection_mask)
{
}

IntersectionDFA::IntersectionDFA(const std::vector<const DFA *> dfas_in)
  : BinaryDFA(dfas_in, intersection_mask)
{
}
