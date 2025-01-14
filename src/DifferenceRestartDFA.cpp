// DifferenceRestartDFA.cpp

#include "DifferenceRestartDFA.h"

DifferenceRestartDFA::DifferenceRestartDFA(const DFA& left_in,
                                 const DFA& right_in)
  : BinaryRestartDFA(left_in, right_in, difference_function)
{
}
