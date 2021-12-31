// PreviousDFA.h

#ifndef PREVIOUS_DFA_H
#define PREVIOUS_DFA_H

#include "DFA.h"
#include "Side.h"
#include "UnionDFA.h"

class PreviousDFA : public UnionDFA
{
 public:
  PreviousDFA(Side, const DFA&);
};

#endif
