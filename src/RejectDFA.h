// RejectDFA.h

#ifndef REJECT_DFA_H
#define REJECT_DFA_H

#include "DedupedDFA.h"

template <int ndim, int... shape_pack>
class RejectDFA : public DedupedDFA<ndim, shape_pack...>
{
 public:

  RejectDFA();
};

#endif
