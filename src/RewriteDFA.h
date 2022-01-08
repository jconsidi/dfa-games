// RewriteDFA.h

#ifndef REWRITE_DFA_H
#define REWRITE_DFA_H

#include <functional>

#include "DFA.h"

template<int ndim, int... shape_pack>
class RewriteDFA : public DFA<ndim, shape_pack...>
{
 public:
  RewriteDFA(const DFA<ndim, shape_pack...>&, std::function<void(int, DFATransitions&)>);
};

#endif
