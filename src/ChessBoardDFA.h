// ChessBoardDFA.h

#ifndef CHESS_BOARD_DFA_H
#define CHESS_BOARD_DFA_H

#include "ChessGame.h"
#include "DedupedDFA.h"

class ChessBoardDFA
: public DedupedDFA<CHESS_DFA_PARAMS>
{
 public:

  ChessBoardDFA(const Board&);
};

#endif
