// chess.h

#ifndef CHESS_H

#include <cstdint>

#define CHESS_MAX_MOVES 256

typedef uint64_t BoardMask;

enum Side {SIDE_WHITE, SIDE_BLACK};
enum Piece {PIECE_KING, PIECE_QUEEN, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK, PIECE_PAWN, PIECE_MAX};

class Board
{
 private:
  BoardMask pieces = 0;
  BoardMask side_pieces[2] = {0};
  BoardMask side_piece_types[2][PIECE_MAX] = {{0}};

  Side side_to_move = SIDE_WHITE;

  bool check_between(int i, int j) const;
  bool try_move(int from, int to, Board *move_out) const;
  
 public:
  Board() {}
  Board(const char *fen_string);

  int generate_moves(Board moves_out[CHESS_MAX_MOVES]) const;
  bool is_in_check(Side side) const;
  
  friend std::ostream& operator<<(std::ostream& os, const Board& board);
};

#endif
