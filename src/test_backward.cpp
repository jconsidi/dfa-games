// test_backward.cpp

#include <iostream>
#include <map>
#include <string>

#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_backward GAME_NAME [PLY_MAX]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 1;
  std::cout << "PLY MAX: " << ply_max << std::endl;

  DFAString initial_position = game->get_position_initial();
  std::cout << "INITIAL POSITION:" << std::endl;
  std::cout << game->position_to_string(initial_position) << std::endl;

  shared_dfa_ptr test_positions;
  if(ply_max % 2)
    {
      test_positions = game->get_positions_winning(0, ply_max);
    }
  else
    {
      test_positions = game->get_positions_losing(0, ply_max);
    }

  return 0;
}
