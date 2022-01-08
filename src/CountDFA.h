// CountDFA.h

#ifndef COUNT_DFA_H
#define COUNT_DFA_H

#include "DFA.h"

template<int ndim, int... shape_pack>
class CountDFA : public DFA<ndim, shape_pack...>
{
 public:

  CountDFA(int num_pieces);
};

#endif
