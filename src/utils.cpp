// util.cpp

#include "chess.h"

uint64_t perft(const Board& board, int depth)
{
  if(depth <= 0)
    return 1;
  
  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);
  if(depth == 1)
    return num_moves;

  uint64_t output = 0;
  for(uint64_t i = 0; i < num_moves; ++i)
    {
      output += perft(moves[i], depth - 1);
    }
  return output;
}
