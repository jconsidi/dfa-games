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

  virtual MoveGraph build_move_graph(int) const;

  shared_dfa_ptr get_positions_basic() const;
  shared_dfa_ptr get_positions_can_move_basic(int, int) const;
  shared_dfa_ptr get_positions_can_move_capture(int, int) const;
  shared_dfa_ptr get_positions_king(int) const;

public:

  ChessGame();

  static shared_dfa_ptr from_board(const Board& board);
  static DFAString from_board_to_dfa_string(const Board& board);

  virtual DFAString get_position_initial() const;

  shared_dfa_ptr get_positions_check(int) const;
  virtual shared_dfa_ptr get_positions_lost(int) const;
  shared_dfa_ptr get_positions_threat(int, int) const;
  virtual shared_dfa_ptr get_positions_won(int) const;

  Board position_to_board(int, const DFAString&) const;
  virtual std::string position_to_string(const DFAString&) const;
};

#endif
