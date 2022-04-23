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

  typename T::shared_dfa_ptr lost_positions[2];
  for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
    {
      lost_positions[side_to_move] = tictactoe.get_lost_positions(side_to_move);
    }

  typename T::shared_dfa_ptr current_positions = initial_positions;
  int current_size = current_positions->size();
  int current_states = current_positions->states();
  for(int previous_ply = 0; previous_ply <= n * n; ++previous_ply)
    {
      int side_to_move = previous_ply % 2;

      typename T::shared_dfa_ptr previous_positions = current_positions;
      int previous_size = current_size;

      bool previous_wins_expected = previous_ply >= n + (n - 1);

      typename T::shared_dfa_ptr just_lost_positions(new typename T::intersection_dfa_type(*(lost_positions[side_to_move]),
											   *previous_positions));
      int just_lost_size = just_lost_positions->size();
      bool previous_wins = just_lost_size > 0;
      assert(previous_wins == previous_wins_expected);

      int current_ply = previous_ply + 1;

      current_positions = tictactoe.get_moves_forward(side_to_move, current_positions);
      current_size = current_positions->size();
      current_states = current_positions->states();

      std::cout << "  ply " << current_ply << ": " << current_size << " positions, " << current_states << " states" << std::endl;

      // move counts by side after current move
      int moves_0 = (current_ply + 1) / 2;
      int moves_1 = (current_ply + 0) / 2;
      int moves_this_side = (side_to_move == 0) ? moves_0 : moves_1;

      // construct bounding set based on just piece counts. if there were no wins yet, this will be exact.
      typename T::shared_dfa_ptr count_positions(new typename T::intersection_dfa_type(typename T::count_character_dfa_type(0 + 1, moves_0),
										       typename T::count_character_dfa_type(1 + 1, moves_1)));

      // make sure no boards have the wrong number of moves by each side.
      typename T::shared_dfa_ptr wrong_counts(new typename T::difference_dfa_type(*current_positions, *count_positions));
      assert(wrong_counts->size() == 0);

      if(!previous_wins)
	{
	  // if no previous wins, then the count positions are exactly
	  // the expected positions.

	  assert(current_size == count_positions->size());
	}
      else if(moves_this_side <= n)
	{
	  // this formula only works up to n moves by the side, since
	  // a win on this move could be permutated to winning
	  // earlier.
	  int expected_size = (previous_size - just_lost_size) * (n2 - previous_ply) / moves_this_side;
	  assert(current_size == expected_size);
	}

      if(current_size == 0)
	{
	  // stop if no more games going this long
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
