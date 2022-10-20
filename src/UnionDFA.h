// UnionDFA.h

#ifndef UNION_DFA_H
#define UNION_DFA_H

#include "BinaryDFA.h"

template<int ndim, int... shape_pack>
class UnionDFA : public BinaryDFA<ndim, shape_pack...>
{
  static dfa_state_t union_mask(dfa_state_t left_mask, dfa_state_t right_mask);

 public:

  UnionDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&);
};

#endif
