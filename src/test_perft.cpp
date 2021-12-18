// test_perft.cpp

#include <cstdint>
#include <iostream>
#include <vector>

#include "chess.h"

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
