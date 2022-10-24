// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <map>
#include <utility>
#include <vector>

#include "DFA.h"

typedef dfa_state_t (*leaf_func_t)(dfa_state_t, dfa_state_t);

template <int ndim, int... shape_pack>
class BinaryDFA : public DFA<ndim, shape_pack...>
{
  void build_quadratic_mmap(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, leaf_func_t);

public:

  BinaryDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, leaf_func_t);
};

#endif
