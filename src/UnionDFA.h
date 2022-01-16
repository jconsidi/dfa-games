// UnionDFA.h

#ifndef UNION_DFA_H
#define UNION_DFA_H

#include <vector>

#include "BinaryDFA.h"

template<int ndim, int... shape_pack>
class UnionDFA : public BinaryDFA<ndim, shape_pack...>
{
  static uint64_t union_mask(uint64_t left_mask, uint64_t right_mask);

 public:

  UnionDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&);
  UnionDFA(const std::vector<const DFA<ndim, shape_pack...> *>&);
  UnionDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>&);
};

#endif
