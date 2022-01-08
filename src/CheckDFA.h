// CheckDFA.h

#ifndef CHECK_DFA_H
#define CHECK_DFA_H

#include "ChessDFA.h"
#include "DFA.h"
#include "Side.h"
#include "UnionDFA.h"

class CheckDFA : public ChessUnionDFA
{
  static std::vector<const ChessDFA *> get_king_threats(Side);

  CheckDFA(Side);

 public:

  static const CheckDFA *get_singleton(Side);
};

#endif
