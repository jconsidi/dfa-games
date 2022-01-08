// LegalDFA.h

#ifndef LEGAL_DFA_H
#define LEGAL_DFA_H

#include <vector>

#include "ChessDFA.h"
#include "IntersectionDFA.h"

class LegalDFA : public ChessIntersectionDFA
{
  static std::vector<const ChessDFA *> get_legal_conditions();

 public:
  LegalDFA();
};

#endif
