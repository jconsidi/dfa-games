// CheckDFA.h

#ifndef CHECK_DFA_H
#define CHECK_DFA_H

#include "DFA.h"
#include "Side.h"
#include "UnionDFA.h"

class CheckDFA : public UnionDFA
{
  static std::vector<const DFA *> get_king_threats(Side);

  CheckDFA(Side);

 public:

  static const CheckDFA *get_singleton(Side);
};

#endif
