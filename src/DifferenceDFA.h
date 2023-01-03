// DifferenceDFA.h

#ifndef DIFFERENCE_DFA_H
#define DIFFERENCE_DFA_H

#include "BinaryDFA.h"

class DifferenceDFA : public BinaryDFA
{
  static dfa_state_t difference_mask(dfa_state_t left_mask, dfa_state_t right_mask);

public:

  DifferenceDFA(const DFA&, const DFA&);
};

#endif
