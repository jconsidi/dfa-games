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

  shared_dfa_ptr get_lost_condition(int side_to_move, int x_start, int y_start, int x_delta, int y_delta) const;

 public:

  virtual shared_dfa_ptr build_positions_lost(int) const;

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
