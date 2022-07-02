// ExplicitDFA.h

#ifndef EXPLICIT_DFA_H
#define EXPLICIT_DFA_H

#include "DFA.h"

template <int ndim, int... shape_pack>
class ExplicitDFA
  : public DFA<ndim, shape_pack...>
{
 protected:

  ExplicitDFA();

  void set_state(int, dfa_state_t, const DFATransitions&);
  void set_state(int, dfa_state_t, std::function<dfa_state_t(int)>);
};

#endif
