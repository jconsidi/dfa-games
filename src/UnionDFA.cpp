// UnionDFA.cpp

#include "UnionDFA.h"

#include <iostream>

static uint64_t union_mask(uint64_t left_mask, uint64_t right_mask)
{
  return left_mask | right_mask;
}

UnionDFA::UnionDFA(const DFA& left_in, const DFA& right_in)
  : BinaryDFA(left_in, right_in, union_mask)
{
}

UnionDFA::UnionDFA(const std::vector<const DFA *> dfas_in)
  : BinaryDFA(dfas_in, union_mask)
{
}
