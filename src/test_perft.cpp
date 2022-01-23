// test_perft.cpp

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "chess.h"
#include "PerftTestCases.h"

void test(std::string name, std::string fen, const std::vector<uint64_t>& expected_outputs)
{
  Board board(fen);
  std::cout << "checking " << name << std::endl;
  std::cout << " FEN = " << fen << std::endl;

  for(int i = 0; i < expected_outputs.size(); ++i)
    {
      int depth = i + 1;
      std::cout << " depth " << depth << std::endl;

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
      // en-passant tests (no kings)
      test("en-passant 1", "8/8/8/8/1p6/8/P7/8 w KQkq - 0 1", std::vector<uint64_t>({2, 4}));
      test("en-passant 2", "8/8/8/8/2p5/8/1P6/8 w KQkq - 0 1", std::vector<uint64_t>({2, 4}));
      test("en-passant 3", "8/1p6/8/P7/8/8/8/8 b KQkq - 0 1", std::vector<uint64_t>({2, 4}));

      for(int i = 0; i < perft_test_cases.size(); ++i)
	{
	  const PerftTestCase& test_case = perft_test_cases.at(i);
	  const std::string& name = std::get<0>(test_case);
	  const std::string& fen = std::get<1>(test_case);
	  const std::vector<uint64_t>& expected_outputs = std::get<2>(test_case);

	  test(name, fen, expected_outputs);
	}
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }

  return 0;
}
