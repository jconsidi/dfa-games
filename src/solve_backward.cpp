// solve_backward.cpp

#include <iostream>
#include <map>
#include <string>

#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: solve_backward GAME_NAME [PLY_MAX]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 100;
  std::cout << "PLY MAX: " << ply_max << std::endl;

  DFAString initial_position = game->get_position_initial();
  std::cout << "INITIAL POSITION:" << std::endl;
  std::cout << game->position_to_string(initial_position) << std::endl;

  for(int ply = 0; ply <= ply_max; ++ply)
    {
      shared_dfa_ptr winning = game->get_positions_winning(0, ply);
      if(winning->contains(initial_position))
        {
          std::cout << "P1 wins in " << ply << " ply." << std::endl;
          return 0;
        }

      shared_dfa_ptr losing = game->get_positions_losing(0, ply);
      if(losing->contains(initial_position))
        {
          std::cout << "P1 loses in " << ply << " ply." << std::endl;
          return 0;
        }
    }

  std::cerr << "solution not found within " << ply_max << " ply." << std::endl;

  return 1;
}
