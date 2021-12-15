// test_generate_moves.cpp

#include <cstdint>
#include <iostream>

#include "chess.h"

int main()
{
  Board board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);

  for(int i = 0; i < num_moves; ++i)
    {
      std::cout << moves[i] << std::endl;
    }

  return 0;
}
