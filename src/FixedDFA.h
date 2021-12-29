// FixedDFA.h

#ifndef FIXED_DFA_H
#define FIXED_DFA_H

#include "DFA.h"

class FixedDFA : public DFA
{
 public:
  FixedDFA(int, DFACharacter);
};

#endif
