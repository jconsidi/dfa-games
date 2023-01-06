// test_amazons_forward.cpp

#include <iostream>

#include "AmazonsGame.h"

void test(int width, int height)
{
  AmazonsGame game(width, height);

  auto initial_positions = game.get_positions_initial();
  assert(initial_positions->size() == 1);

  for(auto iter = initial_positions->cbegin();
      iter < initial_positions->cend();
      ++iter)
    {
      std::cout << game.position_to_string(*iter) << std::endl;
    }

  auto first_moves = game.get_moves_forward(0, initial_positions);
  std::cout << first_moves->size() << " positions after first move." << std::endl;

  auto second_moves = game.get_moves_forward(1, first_moves);
  std::cout << second_moves->size() << " positions after second move." << std::endl;
}

int main()
{
  test(4, 4);
  test(5, 5);
  test(6, 6);
  test(8, 8);
  test(10, 10);

  return 0;
}
