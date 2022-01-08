// ReverseMoveDFA.h

#ifndef REVERSE_MOVE_DFA_H
#define REVERSE_MOVE_DFA_H

#include "ChessDFA.h"
#include "DFA.h"

class ReverseMoveDFA : public ChessRewriteDFA
{
 public:
  ReverseMoveDFA(const ChessDFA&, int, int, int);
};

#endif
