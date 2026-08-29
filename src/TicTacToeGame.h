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

  int calculate_layer(int r, int c) const {return r * n + c;}
  
 public:

  TicTacToeGame(int);

  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
  virtual std::optional<int> validate_result(int, const DFAString&) const;
};

#endif
