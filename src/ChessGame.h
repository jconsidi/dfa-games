// ChessGame.h

#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include "Board.h"
#include "ChessDFAParams.h"
#include "Game.h"

class ChessGame
  : public Game
{
private:

  shared_dfa_ptr get_positions_basic() const;
  shared_dfa_ptr get_positions_king(int) const;

  virtual phase_vector get_phases_internal(int) const;

public:

  ChessGame();

  static shared_dfa_ptr from_board(const Board& board);
  static DFAString from_board_to_dfa_string(const Board& board);

  shared_dfa_ptr get_positions_check(int) const;
  virtual shared_dfa_ptr get_positions_initial() const;
  virtual shared_dfa_ptr get_positions_lost(int) const;
  shared_dfa_ptr get_positions_threat(int, int) const;
  virtual shared_dfa_ptr get_positions_won(int) const;

  Board position_to_board(int, const DFAString&) const;
  virtual std::string position_to_string(const DFAString&) const;
};

#endif
