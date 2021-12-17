// perft.cpp

#include <cstdint>
#include <iostream>

#include "chess.h"

const char *FEN_DEFAULT = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const uint64_t PERFT_EXPECTED[] = {1, 20, 400, 8902, 197281, 4865609};

uint64_t perft(const Board& board, int depth);

int main(int argc, char **argv)
{
  const char *fen = (argc >= 2) ? argv[1] : FEN_DEFAULT;

  Board board(fen);
  std::cout << board;

  for(int depth = 0; depth < 6; ++depth)
    {
      uint64_t output = perft(board, depth);
      std::cout << "depth " << depth << ": " << output << std::endl;

      uint64_t output_expected = 0ULL;
      if((fen == FEN_DEFAULT) && (depth < sizeof(PERFT_EXPECTED) / sizeof(uint64_t)))
	{
	  output_expected = PERFT_EXPECTED[depth];
	}
      // LATER: allow expected output on command line

      if((output_expected) && (output != output_expected))
	{
	  std::cerr << "depth " << depth << ": " << output_expected << " expected" << std::endl;
	  return 1;
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
