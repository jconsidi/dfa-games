// RejectDFA.h

#ifndef REJECT_DFA_H
#define REJECT_DFA_H

#include "ExplicitDFA.h"

template <int ndim, int... shape_pack>
class RejectDFA : public ExplicitDFA<ndim, shape_pack...>
{
 public:

  RejectDFA();
};

#endif
