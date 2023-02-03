// test_amazons_forward.cpp

#include <iostream>

#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_forward GAME_NAME\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  auto initial_positions = game->get_positions_initial();
  assert(initial_positions->size() == 1);

  for(auto iter = initial_positions->cbegin();
      iter < initial_positions->cend();
      ++iter)
    {
      std::cout << game->position_to_string(*iter) << std::endl;
    }

  auto first_moves = game->get_moves_forward(0, initial_positions);
  std::cout << first_moves->size() << " positions after first move." << std::endl;

  auto second_moves = game->get_moves_forward(1, first_moves);
  std::cout << second_moves->size() << " positions after second move." << std::endl;

  return 0;
}
