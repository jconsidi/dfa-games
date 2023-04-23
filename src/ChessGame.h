// ChessGame.h

#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include <utility>

#include "Board.h"
#include "ChessDFAParams.h"
#include "Game.h"
#include "MoveSet.h"

class ChessGame
  : public Game
{
private:

  int max_pieces;

  int _calc_layer(int rank, int file) const;
  int _calc_square(int rank, int file) const;
  change_vector _get_changes() const;
  std::vector<std::tuple<int, int, int, int, std::vector<int>, bool>> _get_choices_basic(int) const;
  void _get_choices_basic_helper(std::vector<std::tuple<int, int, int, int, std::vector<int>, bool>>&, int, int, int, const MoveSet&) const;
  void _get_choices_basic_pawns(std::vector<std::tuple<int, int, int, int, std::vector<int>, bool>>&, int) const;
  std::vector<std::pair<int, int>> _get_choices_from(int) const;
  std::vector<std::pair<int, int>> _get_choices_to(int) const;
  std::vector<std::tuple<int, int, int>> _get_choices_to_prev(int) const;
  std::string _get_name_from(int, int) const;
  std::string _get_name_to(int, int) const;
  std::string _get_name_to_prev(int, int, int) const;
  std::vector<int> _get_squares_between(int, int) const;

  virtual MoveGraph build_move_graph(int) const;

  shared_dfa_ptr get_positions_can_move_basic(int, int) const;
  shared_dfa_ptr get_positions_can_move_capture(int, int) const;
  shared_dfa_ptr get_positions_king(int) const;
  shared_dfa_ptr get_positions_legal(int) const;
  shared_dfa_ptr get_positions_legal_shared() const;

public:

  ChessGame();
  ChessGame(int);

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
