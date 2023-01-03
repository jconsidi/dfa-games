// TicTacToeGame.h

#ifndef TICTACTOE_GAME_H
#define TICTACTOE_GAME_H

#include <string>

#include "Game.h"

class TicTacToeGame
  : public Game
{
private:
  int n;

 public:

  TicTacToeGame(int);

 private:

  virtual shared_dfa_ptr get_positions_initial() const;
  virtual shared_dfa_ptr get_positions_lost(int) const;
  virtual shared_dfa_ptr get_positions_won(int) const;

  virtual phase_vector get_phases_internal(int) const;

  shared_dfa_ptr get_lost_condition(int side_to_move, int x_start, int y_start, int x_delta, int y_delta) const;

 public:

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
