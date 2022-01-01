// ReverseMoveDFA.h

#ifndef REVERSE_MOVE_DFA_H
#define REVERSE_MOVE_DFA_H

#include "DFA.h"
#include "RewriteDFA.h"

class ReverseMoveDFA : public RewriteDFA
{
 public:
  ReverseMoveDFA(const DFA&, DFACharacter, int, int);
};

#endif
