// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <map>
#include <utility>
#include <vector>

#include "DFA.h"
#include "ExplicitDFA.h"

typedef dfa_state_t (*leaf_func_t)(dfa_state_t, dfa_state_t);

template <int ndim, int... shape_pack>
class BinaryDFA : public ExplicitDFA<ndim, shape_pack...>
{
  void binary_build(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, leaf_func_t);

public:

  BinaryDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, leaf_func_t);
  BinaryDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>&, leaf_func_t);
};

#endif
