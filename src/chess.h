// chess.h

#ifndef CHESS_H

#include <cstdint>
#include <iostream>

#define CHESS_MAX_MOVES 256
#define INITIAL_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

typedef uint64_t BoardMask;

enum Side {SIDE_WHITE, SIDE_BLACK};
enum Piece {PIECE_KING, PIECE_QUEEN, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK, PIECE_PAWN, PIECE_MAX};

class Board
{
 private:
  BoardMask pieces = 0;
  BoardMask pieces_by_side[2] = {0};
  BoardMask pieces_by_side_type[2][PIECE_MAX] = {{0}};

  Side side_to_move = SIDE_WHITE;
  int en_passant_file = -1;

  bool check_between(int i, int j) const;
  bool finish_move();
  void move_piece(int from_index, int to_index);
  void start_move(Board *move_out) const;
  bool try_move(int from_index, int to_index, Board *move_out) const;
  
 public:
  Board() {}
  Board(const char *fen_string);

  int generate_moves(Board moves_out[CHESS_MAX_MOVES]) const;
  bool is_attacked(Side defending_side, int defending_index) const;
  bool is_in_check(Side defending_side) const;
  
  friend std::ostream& operator<<(std::ostream& os, const Board& board);
  friend std::string uci_move(const Board& before, const Board& after);
};

// utility functions

uint64_t perft(const Board& board, int depth);

#endif
