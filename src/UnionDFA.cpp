// UnionDFA.cpp

#include "UnionDFA.h"

#include <iostream>

UnionDFA::UnionDFA(const DFA& left_in, const DFA& right_in)
  : BinaryDFA(left_in, right_in, [](uint64_t left_mask, uint64_t right_mask)
  {
    return left_mask | right_mask;
  })
{
}
