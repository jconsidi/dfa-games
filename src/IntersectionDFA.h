// IntersectionDFA.h

#ifndef INTERSECTION_DFA_H
#define INTERSECTION_DFA_H

#include "BinaryDFA.h"

class IntersectionDFA : public BinaryDFA
{
  static dfa_state_t intersection_mask(dfa_state_t left_mask, dfa_state_t right_mask);

 public:

  IntersectionDFA(const DFA&, const DFA&);
};

#endif
