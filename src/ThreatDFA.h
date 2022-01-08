// ThreatDFA.h

#ifndef THREAT_DFA_H
#define THREAT_DFA_H

#include <vector>

#include "ChessDFA.h"
#include "Side.h"
#include "UnionDFA.h"

class ThreatDFA : public ChessUnionDFA
{
  static std::vector<const ChessDFA *> get_threats(Side threatened_side, int threatened_square);

 public:

  ThreatDFA(Side, int);
};

#endif
