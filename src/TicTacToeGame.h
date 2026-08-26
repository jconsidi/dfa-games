// TicTacToeGame.h

#ifndef TICTACTOE_GAME_H
#define TICTACTOE_GAME_H

#include <string>

#include "ConfigGame.h"

class TicTacToeGame
  : public ConfigGame
{
private:
  int n;

 public:

  TicTacToeGame(int);

 private:

  virtual MoveGraph build_move_graph(int) const;

 public:

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
