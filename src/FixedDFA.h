// FixedDFA.h

#ifndef FIXED_DFA_H
#define FIXED_DFA_H

#include <vector>

#include "DFA.h"

template <int ndim, int... shape_pack>
class FixedDFA : public DFA<ndim, shape_pack...>
{
 public:
  FixedDFA(int, int);
};

#endif
