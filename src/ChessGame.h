// ChessGame.h

#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include <utility>

#include "Board.h"
#include "ChessDFAParams.h"
#include "Game.h"
#include "MoveSet.h"

#if CHESS_SQUARE_OFFSET == 1
#define CHESS_SIDE_TO_MOVE_KING_LAYER 0
#elif CHESS_SQUARE_OFFSET == 2
#define CHESS_SIDE_TO_MOVE_KING_LAYER side_to_move
#endif

class ChessGame
  : public Game
{
private:

  int _calc_layer(int rank, int file) const;
  int _calc_square(int rank, int file) const;
  int _calc_castle_rank(int side) const;
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
#ifdef CHESS_SIDE_TO_MOVE_KING_LAYER
  shared_dfa_ptr get_positions_king(int) const;
#endif
  shared_dfa_ptr get_positions_legal_shared() const;
  shared_dfa_ptr get_positions_not_check(int) const;

public:

  ChessGame();

  virtual shared_dfa_ptr build_positions_lost(int) const;

  static shared_dfa_ptr from_board(const Board& board);
  static DFAString from_board_to_dfa_string(const Board& board);

  virtual DFAString get_position_initial() const;

  shared_dfa_ptr get_positions_check(int) const;
  shared_dfa_ptr get_positions_legal(int) const;
  shared_dfa_ptr get_positions_threat(int, int) const;

  Board position_to_board(int, const DFAString&) const;
  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, DFAString) const;
  virtual std::optional<int> validate_result(int, DFAString) const;
};

#endif
