// ForcedDFA.h

#ifndef FORCED_DFA_H
#define FORCED_DFA_H

#include "ChessDFA.h"
#include "IntersectionDFA.h"
#include "Side.h"

class ForcedDFA : public ChessIntersectionDFA
{
 public:
  ForcedDFA(Side, const ChessDFA&);
};

#endif
