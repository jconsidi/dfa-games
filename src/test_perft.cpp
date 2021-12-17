// test_perft.cpp

#include <cstdint>
#include <iostream>
#include <vector>

#include "chess.h"

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

void test(const char *fen, std::vector<uint64_t> expected_outputs)
{
  Board board(fen);
  std::cout << "checking " << fen << std::endl;
  
  for(int i = 0; i < expected_outputs.size(); ++i)
    {
      int depth = i + 1;

      uint64_t actual_output = perft(board, depth);
      uint64_t expected_output = expected_outputs[i];

      if(actual_output != expected_output)
	{
	  std::cerr << "depth " << depth << ": expected = " << expected_output << std::endl;
	  std::cerr << "depth " << depth << ":   actual = " << actual_output << std::endl;

	  throw std::logic_error("perft failed");
	}
    }
}

int main()
{
  try
    {
      // initial position
      test("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", std::vector<uint64_t>({20, 400, 8902, 197281}));
    }
  catch(std::logic_error)
    {
      return 1;
    }
  
  return 0;
}
