// generate_moves.cpp

#include <cstdint>
#include <iostream>

#include "utils.h"

int main(int argc, char **argv)
{
  const char *fen = (argc >= 2) ? argv[1] : INITIAL_FEN;
  Board board(fen);
  std::cout << board << std::endl;

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);

  for(int i = 0; i < num_moves; ++i)
    {
      std::cout << "MOVE " << i << std::endl;
      std::cout << moves[i] << std::endl;
    }

  return 0;
}
