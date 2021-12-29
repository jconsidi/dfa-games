// CheckDFA.h

#ifndef CHECK_DFA_H
#define CHECK_DFA_H

#include "DFA.h"
#include "Side.h"

class CheckDFA : public DFA
{
 public:
  CheckDFA(Side);
};

#endif
