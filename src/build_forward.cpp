// test_forward.cpp

#include <cstdlib>
#include <iostream>

#include "build_utils.h"
#include "game_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_forward GAME_NAME [depth]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 100;

  auto initial_positions = game->get_positions_initial();
  assert(initial_positions->size() == 1);

  std::cout << game->position_to_string(game->get_position_initial()) << std::endl;

  build_forward(*game, ply_max);

  return 0;
}
