// DifferenceRestartDFA.h

#ifndef DIFFERENCE_RESTART_DFA_H
#define DIFFERENCE_RESTART_DFA_H

#include "BinaryRestartDFA.h"
#include "DifferenceDFA.h"

class DifferenceRestartDFA : public BinaryRestartDFA
{
 public:

  DifferenceRestartDFA(const DFA&, const DFA&);
};

#endif
