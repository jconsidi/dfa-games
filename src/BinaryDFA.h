// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <map>
#include <utility>
#include <vector>

#include "BinaryFunction.h"
#include "DFA.h"

class BinaryDFA : public DFA
{
  void build_linear(const DFA&, const DFA&, const BinaryFunction&);
  void build_quadratic_mmap(const DFA&, const DFA&, const BinaryFunction&);

public:

  BinaryDFA(const DFA&, const DFA&, const BinaryFunction&);
};

#endif
