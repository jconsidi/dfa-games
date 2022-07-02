// AcceptDFA.h

#ifndef ACCEPT_DFA_H
#define ACCEPT_DFA_H

#include "ExplicitDFA.h"

template <int ndim, int... shape_pack>
class AcceptDFA : public ExplicitDFA<ndim, shape_pack...>
{
 public:

  AcceptDFA();
};

#endif
