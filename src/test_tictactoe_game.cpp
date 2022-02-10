// test_tictactoe_game.cpp

#include <iostream>

#include "TicTacToeGame.h"
#include "test_utils.h"

template<int n, class T>
int test()
{
  std::cout << "TESTING " << n << "x" << n << std::endl;

  T tictactoe;

  // start tictactoe specific tests

  int n2 = n * n;

  // test forward move generation up through first winning positions
  // for each side.

  typename T::shared_dfa_ptr initial_positions = tictactoe.get_initial_positions();
  assert(initial_positions);
  assert(initial_positions->size() == 1);

  std::cout << " perft_u" << std::endl;

  typename T::shared_dfa_ptr current_positions = initial_positions;
  int current_size = current_positions->size();
  int current_states = current_positions->states();
  for(int previous_ply = 0; previous_ply < 2 * n; ++previous_ply)
    {
      int side_to_move = previous_ply % 2;

      typename T::shared_dfa_ptr previous_positions = current_positions;
      int previous_size = current_size;

      typename T::shared_dfa_ptr just_lost_positions = tictactoe.get_lost_positions(side_to_move, previous_positions);
      int just_lost_size = just_lost_positions->size();
      if((previous_ply + 1) / 2 >= n)
	{
	  // other player had enough moves for a win
	  assert(just_lost_size > 0);
	}
      else
	{
	  assert(just_lost_size == 0);
	}

      int expected_size = (previous_size - just_lost_size) * (n2 - previous_ply) / (previous_ply / 2 + 1);

      current_positions = tictactoe.get_moves_forward(side_to_move, current_positions);
      current_size = current_positions->size();
      current_states = current_positions->states();

      std::cout << "  ply " << (previous_ply + 1) << ": " << current_size << " positions, " << current_states << " states" << std::endl;
      assert(current_size == expected_size);

      if(current_size == 0)
	{
	  break;
	}
    }

  // first player should never lose via strategy stealing argument.

  std::cout << " retrograde analysis" << std::endl;
  typename T::shared_dfa_ptr initial_winning = tictactoe.get_winning_positions(0, n*n, initial_positions);
  if(n <= 2)
    {
      // first player always wins
      assert(initial_winning->size() == 1);
      std::cout << "  confirmed win" << std::endl;
    }
  else
    {
      // draws with perfect play
      assert(initial_winning->size() == 0);
      std::cout << "  rejected win" << std::endl;
    }

  // generic tests

  test_game(tictactoe);

  // done

  return 0;
}

int main()
{
  test<2, TicTacToe2Game>();
  test<3, TicTacToe3Game>();
#if 0
  test<4, TicTacToe4Game>();
#endif

  return 0;
}
