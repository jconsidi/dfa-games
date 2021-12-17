// perft.cpp

#include <cstdint>
#include <iostream>

#include "chess.h"

const uint64_t PERFT_EXPECTED[] = {1, 20, 400, 8902, 197281, 4865609};

uint64_t perft(const Board& board, int depth);

int main()
{
  Board board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  std::cout << board;

  for(int depth = 0; depth < 5; ++depth)
    {
      uint64_t output = perft(board, depth);
      std::cout << "depth " << depth << ": " << output << std::endl;

      if(depth < sizeof(PERFT_EXPECTED) / sizeof(uint64_t))
	{
	  uint64_t output_expected = PERFT_EXPECTED[depth];
	  if(output != output_expected)
	    {
	      std::cerr << "depth " << depth << ": got " << output << " but expected " << output_expected << std::endl;
	      return 1;
	    }
	}
    }

  return 0;
}

uint64_t perft(const Board& board, int depth)
{
  if(depth <= 0)
    return 1;
  
  Board moves[CHESS_MAX_MOVES];
  uint64_t num_moves = board.generate_moves(moves);
  if(depth == 1)
    return num_moves;

  uint64_t output = 0;
  for(uint64_t i = 0; i < num_moves; ++i)
    {
      output += perft(moves[i], depth - 1);
    }
  return output;
}
