// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <map>
#include <utility>
#include <vector>

#include "BinaryFunction.h"
#include "DFA.h"

template <int ndim, int... shape_pack>
class BinaryDFA : public DFA<ndim, shape_pack...>
{
  void build_linear(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, const BinaryFunction&);
  void build_quadratic_mmap(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, const BinaryFunction&);

public:

  BinaryDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, const BinaryFunction&);
};

#endif
