// ChessBoardDFA.h

#ifndef CHESS_BOARD_DFA_H
#define CHESS_BOARD_DFA_H

#include "ChessGame.h"

class ChessBoardDFA
: public ChessGame::dfa_type
{
 public:

  ChessBoardDFA(const Board&);
};

#endif
