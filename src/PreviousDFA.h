// PreviousDFA.h

#ifndef PREVIOUS_DFA_H
#define PREVIOUS_DFA_H

#include "DFA.h"
#include "Side.h"

class PreviousDFA : public DFA
{
 public:
  PreviousDFA(Side, const DFA&);
};

#endif
