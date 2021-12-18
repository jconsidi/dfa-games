// divide.cpp

#include <stdlib.h>

#include <bit>
#include <cstdint>
#include <iostream>
#include <string>

#include "chess.h"

std::string mask_to_square(BoardMask mask)
{
  // map mask to a square name. assumes only one bit is set in the
  // mask.

  int index = std::countr_zero(mask);
  assert(index < 64);
  
  int rank = index / 8;
  int file = index % 8;

  std::string output;
  output += 'a' + file;
  output += '1' + (7 - rank);

  return output;
}

std::string uci_move(const Board& before, const Board& after)
{
  // approximate UCI move string. will fail for castling and promotion.

  BoardMask flipped = before.pieces_by_side[before.side_to_move] ^ after.pieces_by_side[before.side_to_move];
  assert(flipped);
  BoardMask from = flipped & before.pieces_by_side[before.side_to_move];
  assert(from);
  BoardMask to = flipped & after.pieces_by_side[before.side_to_move];
  assert(to);

  return mask_to_square(from) + mask_to_square(to);
}

int main(int argc, char **argv)
{
  const char *fen = (argc > 1) ? argv[1] : INITIAL_FEN;
  Board board(fen);

  int depth = (argc > 2) ? atoi(argv[2]) : 4;

  Board moves[CHESS_MAX_MOVES];
  uint64_t num_moves = board.generate_moves(moves);

  for(int i = 0; i < num_moves; ++i)
    {
      std::cout << uci_move(board, moves[i]) << " " << perft(moves[i], depth - 1) << std::endl;
    }
  
  return 0;
}
