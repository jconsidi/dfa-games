// test_tictactoe_game.cpp

#include <iostream>

#include "TicTacToeGame.h"
#include "test_utils.h"

size_t choose(int n, int k)
{
  size_t output = 1;

  // compute n! / (n-k)!
  for(size_t i = n - k + 1; i <= n; ++i)
    {
      output *= i;
    }

  // divide by k!
  for(size_t i = 2; i <= k; ++i)
    {
      output /= i;
    }

  return output;
}

int test(int n)
{
  std::cout << "TESTING " << n << "x" << n << std::endl;

  TicTacToeGame tictactoe(n);

  // start tictactoe specific tests

  int n2 = n * n;

  // calculate positions expected to handoff to generic test code

  std::vector<size_t> positions_expected;
  positions_expected.push_back(1);

  for(int depth = 1; depth <= 2 * n; ++depth)
    {
      int moves_0 = (depth + 1) / 2;
      int moves_1 = (depth + 0) / 2;

      size_t positions_0 = choose(n2, moves_0);
      size_t positions_1 = choose(n2 - moves_0, moves_1);

      size_t positions_0_won = (depth == 2 * n) ? (2 * n + 2) : 0;

      positions_expected.push_back((positions_0 - positions_0_won) * positions_1);
    }

  // TicTacToe is a win for the first player for 2x2 and smaller.

  bool initial_win_expected = n <= 2;

  // run generic tests

  test_game(tictactoe, positions_expected, n2, initial_win_expected);

  return 0;
}

int main()
{
  test(2);
  test(3);
#if 0
  test(4);
#endif

  return 0;
}
