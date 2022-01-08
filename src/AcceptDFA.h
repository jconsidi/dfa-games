// AcceptDFA.h

#ifndef ACCEPT_DFA_H
#define ACCEPT_DFA_H

#include "DFA.h"

template <int ndim, int... shape_pack>
class AcceptDFA : public DFA<ndim, shape_pack...>
{
 public:

  AcceptDFA();
};

#endif
