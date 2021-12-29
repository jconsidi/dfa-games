// ThreatDFA.h

#ifndef THREAT_DFA_H
#define THREAT_DFA_H

#include "DFA.h"
#include "Side.h"

class ThreatDFA : public DFA
{
 public:

  ThreatDFA(Side, int);
};

#endif
