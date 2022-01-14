// RejectDFA.h

#ifndef REJECT_DFA_H
#define REJECT_DFA_H

#include "DFA.h"

template <int ndim, int... shape_pack>
class RejectDFA : public DFA<ndim, shape_pack...>
{
 public:

  RejectDFA();
};

#endif
