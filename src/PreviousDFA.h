// PreviousDFA.h

#ifndef PREVIOUS_DFA_H
#define PREVIOUS_DFA_H

#include "ChessDFA.h"
#include "DFA.h"
#include "Side.h"
#include "UnionDFA.h"

class PreviousDFA : public ChessUnionDFA
{
 public:
  PreviousDFA(Side, const ChessDFA&);
};

#endif
