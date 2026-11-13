// build_backward.cpp

#include <format>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "DFAUtil.h"
#include "game_utils.h"

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

  // Each ply's winning and losing sets are built from the previous ply's,
  // by one of two rules that alternate with the ply's parity when a game
  // has no won or no lost positions (see Game::build_positions_winning and
  // build_positions_losing). Once both sides' sets are unchanged for two
  // consecutive plies, both rules have mapped the sets to themselves, so
  // every later ply is identical too. Equal hashes mean equal DFAs.
  std::vector<std::string> previous_hashes;
  int unchanged_plies = 0;

  for(int ply = 0; ply <= ply_max; ++ply)
    {
      bool finished = true;
      std::vector<std::string> hashes;
      std::vector<shared_dfa_ptr> unknowns;
      for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
	{
	  shared_dfa_ptr losing = game->get_positions_losing(side_to_move, ply);
	  shared_dfa_ptr winning = game->get_positions_winning(side_to_move, ply);
	  shared_dfa_ptr unknown = game->get_positions_unknown(side_to_move, ply);
	  hashes.push_back(losing->get_hash());
	  hashes.push_back(winning->get_hash());
	  unknowns.push_back(unknown);

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
          std::cout << "no unknown positions at ply " << ply << std::endl;
          break;
        }

      unchanged_plies = (hashes == previous_hashes) ? unchanged_plies + 1 : 0;
      previous_hashes = hashes;
      if(unchanged_plies >= 2)
        {
          std::cout << std::format("fixed point at ply {:d}: winning and losing unchanged for both sides since ply {:d}, unknown positions remaining {:g} (side 0) and {:g} (side 1)", ply, ply - 2, unknowns[0]->size(), unknowns[1]->size()) << std::endl;
          break;
        }
    }

  return 0;
}
