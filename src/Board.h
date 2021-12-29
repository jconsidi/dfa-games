// Board.h

#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include <ctype.h>
#include <iostream>

#include "Side.h"

#define CHESS_MAX_MOVES 256

typedef uint64_t BoardMask;
enum Piece {PIECE_KING, PIECE_QUEEN, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK, PIECE_PAWN, PIECE_MAX};

class Board
{
 private:
  BoardMask pieces = 0;
  BoardMask pieces_by_side[2] = {0};
  BoardMask pieces_by_side_type[2][PIECE_MAX] = {{0}};

  Side side_to_move = SIDE_WHITE;
  int en_passant_file = -1;
  BoardMask castling_availability = 0;
  int halfmove_clock = 0;
  int fullmove_number = 1;

  bool check_between(int i, int j) const;
  bool finish_move();
  void move_piece(int from_index, int to_index);
  void start_move(Board &move_out) const;
  bool try_castle(int king_index, int rook_index, Board &move_out) const;
  bool try_move(int from_index, int to_index, Board &move_out) const;

 public:
  Board() {}
  Board(const char *fen_string);

  int count_moves() const;
  int generate_moves(Board moves_out[CHESS_MAX_MOVES]) const;
  bool is_attacked(Side defending_side, int defending_index) const;
  bool is_check() const;
  bool is_check(Side defending_side) const;
  bool is_checkmate() const;
  bool is_draw() const;
  bool is_draw_by_material() const;
  bool is_draw_by_rule() const;
  bool is_final() const;
  bool is_stalemate() const;

  friend std::ostream& operator<<(std::ostream& os, const Board& board);
  friend std::string uci_move(const Board& before, const Board& after);
};

#endif