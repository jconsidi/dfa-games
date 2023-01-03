// UnionDFA.h

#ifndef UNION_DFA_H
#define UNION_DFA_H

#include "BinaryDFA.h"

class UnionDFA : public BinaryDFA
{
  static dfa_state_t union_mask(dfa_state_t left_mask, dfa_state_t right_mask);

 public:

  UnionDFA(const DFA&, const DFA&);
};

#endif
