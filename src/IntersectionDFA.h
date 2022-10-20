// IntersectionDFA.h

#ifndef INTERSECTION_DFA_H
#define INTERSECTION_DFA_H

#include "BinaryDFA.h"

template<int ndim, int... shape_pack>
class IntersectionDFA : public BinaryDFA<ndim, shape_pack...>
{
  static dfa_state_t intersection_mask(dfa_state_t left_mask, dfa_state_t right_mask);

 public:

  IntersectionDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&);
};

#endif
