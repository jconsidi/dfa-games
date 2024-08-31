// BinaryRestartDFA.h

#ifndef BINARY_RESTART_DFA_H
#define BINARY_RESTART_DFA_H

#include "BinaryDFA.h"
#include "DFA.h"

class BinaryRestartDFA : public BinaryDFA
{
 protected:

  BinaryRestartDFA(const DFA&, const DFA&, const BinaryFunction&);
};

#endif
