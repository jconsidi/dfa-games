// DifferenceDFA.cpp

#include "DifferenceDFA.h"

static uint64_t difference_mask(uint64_t left_mask, uint64_t right_mask)
{
  return left_mask & ~right_mask;
}

DifferenceDFA::DifferenceDFA(const DFA& left_in, const DFA& right_in)
  : BinaryDFA(left_in, right_in, difference_mask)
{
}
