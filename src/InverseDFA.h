// InverseDFA.h

#ifndef INVERSE_DFA_H
#define INVERSE_DFA_H

#include "DFA.h"

template<int ndim, int...shape_pack>
class InverseDFA : public DFA<ndim, shape_pack...>
{
 public:
  InverseDFA(const DFA<ndim, shape_pack...>&);
};

#endif
