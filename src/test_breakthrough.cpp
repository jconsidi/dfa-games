// test_breakthrough.cpp

// unit tests for BreakthroughGame

#include <cassert>
#include <iostream>
#include <vector>

#include "BreakthroughGame.h"
#include "DFAUtil.h"

void test_forward(const BreakthroughGame& game, int width, int height)
{
  // deliberately not using get_positions_forward() to avoid cache.

  assert(height >= 4);

  // generate positions

  std::vector<shared_dfa_ptr> positions_by_ply;

  // ply zero : just initial position
  positions_by_ply.push_back(game.get_positions_initial());
  assert(positions_by_ply.at(0)->size() == 1);

  int ply_max = (height > 4) ? (height - 4) : 1;
  for(int ply = 1; ply <= ply_max; ++ply)
    {
      positions_by_ply.push_back(game.get_moves_forward(ply % 2,
							positions_by_ply.back()));
    }

  // check number of positions for each ply

  auto check_ply = [&](int ply, size_t expected_positions)
  {
    assert(positions_by_ply.at(ply)->size() == expected_positions);
  };

  if(height == 4)
    {
      // no rows between players. captures start immediately.
      check_ply(1, width * 2 - 2);
      return;
    }

  size_t positions_1 = width * 3 - 2;
  check_ply(1, positions_1);
  if(height == 5)
    {
      return;
    }

  size_t positions_2 = positions_1 * positions_1;
  check_ply(2, positions_2);
  if(height < 8)
    {
      return;
    }

  // TODO : calculate positions after side's second move
  size_t positions_3 = positions_by_ply.at(3)->size();

  size_t side_0_positions = positions_3 / positions_1;
  size_t positions_4 = side_0_positions * side_0_positions;
  check_ply(4, positions_4);
}

void test_reachable(const BreakthroughGame& game, int width, int height)
{
  // deliberately not using get_positions_reachable() to avoid cache

  auto get_reachable = [&](int side_to_move)
  {
    shared_dfa_ptr ply_0 = DFAUtil::get_accept(game.get_shape());
    shared_dfa_ptr ply_1 = game.get_moves_forward(side_to_move, ply_0);
    shared_dfa_ptr ply_2 = game.get_moves_forward(1 - side_to_move, ply_1);
    return ply_2;
  };

  shared_dfa_ptr reachable_0 = get_reachable(0);
  shared_dfa_ptr reachable_1 = get_reachable(1);

  // initial position is not reachable

  auto check_both_string = [&](const DFAString& position, bool expected)
  {
    std::cout << game.position_to_string(position) << std::endl;

    assert(reachable_0->contains(position) == expected);
    assert(reachable_1->contains(position) == expected);
  };

  DFAString initial_position = game.get_position_initial();
  check_both_string(initial_position, false);

  // generic checking of both
  auto check_both = [&](const std::vector<int>& characters_in, bool expected)
  {
    DFAString position(game.get_shape(), characters_in);
    check_both_string(position, expected);
  };

  // reject cases where both sides got to the end
  for(int col_0 = 0; col_0 < width; ++col_0)
    {
      for(int col_1 = 0; col_1 < width; ++col_1)
	{
	  std::vector<int> both_won(width * height, 0);
	  both_won[(height - 1) * width + col_0] = 1;
	  both_won[0 * width + col_1] = 2;

	  check_both(both_won, false);
	}
    }

  if(height >= 5)
    {
      // setup each side to have their penultimate row filled with their pieces

      std::vector<int> penultimate;
      for(int row = 0; row < height; ++row)
	{
	  int row_character = (row == 1) ? 2 : (row == height - 2) ? 1 : 0;

	  for(int col = 0; col < width; ++col)
	    {
	      penultimate.push_back(row_character);
	    }
	}
      check_both(penultimate, true);
    }
}

void test(int width, int height)
{
  std::cout << "TESTING " << width << "x" << height << std::endl;
  std::cout << std::endl;

  BreakthroughGame game(width, height);

  DFAString initial_position = game.get_position_initial();
  std::cout << game.position_to_string(initial_position) << std::endl;

  test_forward(game, width, height);
  test_reachable(game, width, height);
}

int main()
{
  test(4, 4);
  test(5, 5);
  test(6, 5);
  test(5, 6);
  test(6, 6);
  test(7, 7);
  test(8, 8);

  return 0;
}
