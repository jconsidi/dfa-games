// TicTacToeGame.h

#ifndef TICTACTOE_GAME_H
#define TICTACTOE_GAME_H

#include <string>

#include "ConfigGame.h"
#include "RowMajorOrderBase.h"

class TicTacToeGame
  : public RowMajorOrderBase,
    public ConfigExplicitOutcomeGame
    
{
private:
  int n;

 public:

  TicTacToeGame(int);

  virtual std::string position_to_string(const DFAString&) const;


 protected:

  // The opponent's line is lost and a full board tied; otherwise the moves.
  virtual ValidatedPosition validate_position_impl(int, const DFAString&) const;

  // the side to move's moves, for validate_position_impl
  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;

 private:

  // -1 for the opponent's line, 0 for a full board, otherwise none.
  std::optional<int> get_result(int, const DFAString&) const;
};

#endif
