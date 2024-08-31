// UnionDFA.h

#ifndef UNION_DFA_H
#define UNION_DFA_H

#include "BinaryDFA.h"

class UnionDFA : public BinaryDFA
{
 public:

  UnionDFA(const DFA&, const DFA&);
};

#endif
