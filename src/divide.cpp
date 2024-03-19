// divide.cpp

#include <stdlib.h>

#include <bit>
#include <cstdint>
#include <iostream>
#include <string>

#include "utils.h"

int main(int argc, char **argv)
{
  const char *fen = (argc > 1) ? argv[1] : INITIAL_FEN;
  Board board(fen);

  int depth = (argc > 2) ? atoi(argv[2]) : 4;

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);

  for(int i = 0; i < num_moves; ++i)
    {
      std::cout << uci_move(board, moves[i]) << " " << perft(moves[i], depth - 1) << std::endl;
    }

  return 0;
}
