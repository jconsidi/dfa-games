// UnionDFA.h

#ifndef UNION_DFA_H
#define UNION_DFA_H

#include <vector>

#include "BinaryDFA.h"

class UnionDFA : public BinaryDFA
{
 public:

  UnionDFA(const DFA&, const DFA&);
  UnionDFA(const std::vector<const DFA *>);
};

#endif
