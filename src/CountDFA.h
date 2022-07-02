// CountDFA.h

#ifndef COUNT_DFA_H
#define COUNT_DFA_H

#include "DedupedDFA.h"

template<int ndim, int... shape_pack>
class CountDFA : public DedupedDFA<ndim, shape_pack...>
{
 public:

  CountDFA(int num_pieces);
};

#endif
