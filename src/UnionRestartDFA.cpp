// UnionRestartDFA.cpp

#include "UnionRestartDFA.h"

UnionRestartDFA::UnionRestartDFA(const DFA& left_in,
                                 const DFA& right_in)
  : BinaryRestartDFA(left_in, right_in, union_function)
{
}
