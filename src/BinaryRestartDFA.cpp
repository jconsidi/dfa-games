// BinaryRestartDFA.cpp

#include "BinaryRestartDFA.h"

BinaryRestartDFA::BinaryRestartDFA(const DFA& left_in,
                                   const DFA& right_in,
                                   const BinaryFunction& leaf_func_in)
  : BinaryDFA(left_in.get_shape(), leaf_func_in)
{
  // restart assuming forward pass completed, and all layers participate in backward pass
  build_quadratic_backward(left_in, right_in, get_shape().size());
}
