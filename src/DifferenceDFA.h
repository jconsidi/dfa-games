// DifferenceDFA.h

#ifndef DIFFERENCE_DFA_H
#define DIFFERENCE_DFA_H

#include "BinaryDFA.h"

template<int ndim, int... shape_pack>
class DifferenceDFA : public BinaryDFA<ndim, shape_pack...>
{
  static dfa_state_t difference_mask(dfa_state_t left_mask, dfa_state_t right_mask);

public:

  DifferenceDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&);
};

#endif
