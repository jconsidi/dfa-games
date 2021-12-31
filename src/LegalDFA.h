// LegalDFA.h

#ifndef LEGAL_DFA_H
#define LEGAL_DFA_H

#include <vector>

#include "DFA.h"
#include "IntersectionDFA.h"

class LegalDFA : public IntersectionDFA
{
  static std::vector<const DFA *> get_legal_conditions();

 public:
  LegalDFA();
};

#endif
