// test_perft.cpp

#include <cstdint>
#include <iostream>
#include <vector>

#include "chess.h"

void test(const char *fen, std::vector<uint64_t> expected_outputs)
{
  Board board(fen);
  std::cout << "checking " << fen << std::endl;
  std::cout.flush();

  for(int i = 0; i < expected_outputs.size(); ++i)
    {
      int depth = i + 1;
      std::cout << " depth " << depth << std::endl;
      std::cout.flush();

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
      // en-passant tests
      test("8/8/8/8/1p6/8/P7/8 w KQkq - 0 1", std::vector<uint64_t>({2, 4}));
      test("8/8/8/8/2p5/8/1P6/8 w KQkq - 0 1", std::vector<uint64_t>({2, 4}));
      test("8/1p6/8/P7/8/8/8/8 b KQkq - 0 1", std::vector<uint64_t>({2, 4}));

      // initial position
      test(INITIAL_FEN, std::vector<uint64_t>({20, 400, 8902, 197281, 4865609}));

      // positions from https://www.chessprogramming.org/Perft_Results

      // Position 2 aka Kiwipete
      test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", std::vector<uint64_t>({48, 2039, 97862, 4085603}));

      // Position 3
      test("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", std::vector<uint64_t>({14, 191, 2812, 43238, 674624, 11030083}));

      // Position 4
      test("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", std::vector<uint64_t>({6, 264, 9467, 422333, 15833292}));
      test("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", std::vector<uint64_t>({6, 264, 9467, 422333, 15833292}));

      // Position 5
      test("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", std::vector<uint64_t>({44, 1486, 62379, 2103487}));

      // Position 6
      test("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", std::vector<uint64_t>({46, 2079, 89890, 3894594}));

      // positions from http://www.rocechess.ch/perft.html

      test("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", std::vector<uint64_t>({24, 496, 9483, 182838, 3605103}));
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }

  return 0;
}
