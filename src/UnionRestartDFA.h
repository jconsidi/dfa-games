// UnionRestartDFA.h

#ifndef UNION_RESTART_DFA_H
#define UNION_RESTART_DFA_H

#include "BinaryRestartDFA.h"
#include "UnionDFA.h"

class UnionRestartDFA : public BinaryRestartDFA
{
 public:

  UnionRestartDFA(const DFA&, const DFA&);
};

#endif
