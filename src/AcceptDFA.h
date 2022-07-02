// AcceptDFA.h

#ifndef ACCEPT_DFA_H
#define ACCEPT_DFA_H

#include "DedupedDFA.h"

template <int ndim, int... shape_pack>
class AcceptDFA : public DedupedDFA<ndim, shape_pack...>
{
 public:

  AcceptDFA();
};

#endif
