// ForcedDFA.h

#ifndef FORCED_DFA_H
#define FORCED_DFA_H

#include "DFA.h"
#include "IntersectionDFA.h"
#include "Side.h"

class ForcedDFA : public IntersectionDFA
{
 public:
  ForcedDFA(Side, const DFA&);
};

#endif
