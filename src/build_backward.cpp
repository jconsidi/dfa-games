// build_backward.cpp

#include <format>
#include <iostream>
#include <map>
#include <string>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: build_backward GAME_NAME [PLY_MAX]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 1;
  std::cout << "PLY MAX: " << ply_max << std::endl;

  DFAString initial_position = game->get_position_initial();
  std::cout << "INITIAL POSITION:" << std::endl;
  std::cout << game->position_to_string(initial_position) << std::endl;

  for(int ply = 0; ply <= ply_max; ++ply)
    {
      bool finished = true;
      for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
	{
	  shared_dfa_ptr losing = game->get_positions_losing(side_to_move, ply);
	  shared_dfa_ptr winning = game->get_positions_winning(side_to_move, ply);
	  shared_dfa_ptr unknown = game->get_positions_unknown(side_to_move, ply);

          // exact ply counts

          game->load_or_build(std::format("backward,ply={:03d},side={:d},losing", ply, side_to_move), [&]()
          {
            if(ply == 0)
              {
                return game->get_positions_lost(side_to_move);
              }

            return DFAUtil::get_difference(losing, game->get_positions_losing(side_to_move, ply - 1));
          });

          game->load_or_build(std::format("backward,ply={:03d},side={:d},winning", ply, side_to_move), [&]()
          {
            if(ply == 0)
              {
                return game->get_positions_won(side_to_move);
              }

            return DFAUtil::get_difference(winning, game->get_positions_winning(side_to_move, ply - 1));
          });

          // termination check

          if(!unknown->is_constant(0))
            {
              finished = false;
            }
	}

      if(finished)
        {
          break;
        }
    }

  return 0;
}
