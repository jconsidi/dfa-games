// test_clobber_game.cpp

#include <cassert>
#include <iostream>

#include "ClobberGame.h"
#include "test_utils.h"

void test(int width, int height)
{
  std::cout << "TESTING " << width << "x" << height << std::endl;
  std::cout << std::endl;

  ClobberGame game(width, height);

  DFAString initial_position = game.get_position_initial();
  std::cout << game.position_to_string(initial_position) << std::endl;

  // forward pass

  auto positions = game.get_positions_initial();
  std::cout << "depth 0: " << positions->size() << " positions" << std::endl;

  for(int depth = 0; depth < 4; ++depth)
    {
      positions = game.get_moves_forward(depth % 2, positions);
      std::cout << "depth " << (depth + 1) << ": " << positions->size() << " positions" << std::endl;
    }

  // backward pass

  int ply_max = width * height;

  bool side0_wins = check_win(game, ply_max);
  bool side0_loses = check_loss(game, ply_max);

  std::cout << "side 0 wins: " << side0_wins << std::endl;
  std::cout << "side 0 loses: " << side0_loses << std::endl;

  assert(side0_wins || side0_loses);
}

int main()
{
  test(4, 4);

  return 0;
}
