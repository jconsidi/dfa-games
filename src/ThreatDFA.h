// ThreatDFA.h

#ifndef THREAT_DFA_H
#define THREAT_DFA_H

#include <vector>

#include "Side.h"
#include "UnionDFA.h"

class ThreatDFA : public UnionDFA
{
  static std::vector<const DFA *> get_threats(Side threatened_side, int threatened_square);

 public:

  ThreatDFA(Side, int);
};

#endif
