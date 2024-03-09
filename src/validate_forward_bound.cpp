// validate_forward_bound.cpp

#include <cstdlib>
#include <iostream>

#include "DFAUtil.h"
#include "test_utils.h"
#include "validate_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: validate_forward_bound GAME_NAME [depth]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 100;

  dfa_shape_t shape = game->get_shape();

  shared_dfa_ptr positions_check = game->get_positions_initial();
  assert(positions_check->size() == 1);

  for(int ply = 0; ply <= ply_max; ++ply)
    {
      std::cout << "############################################################" << std::endl;
      std::cout << "PLY " << ply << std::endl;
      std::cout << "############################################################" << std::endl;

      shared_dfa_ptr positions = game->get_positions_forward_bound(ply);
      std::cout << std::endl;

      if(!positions)
        {
          std::cerr << "FORWARD BOUND NOT DEFINED" << std::endl;
          return 1;
        }

      std::cout << "BOUND CHECK" << std::endl;
      if(!validate_subset(positions_check, positions))
        {
          std::cerr << "BOUND CHECK FAILED" << std::endl;
          return 1;
        }

      if(positions->size() == 0)
        {
          break;
        }

      positions_check = game->get_moves_forward(ply % 2, positions);
    }

  return 0;
}
