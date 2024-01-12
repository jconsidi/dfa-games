// stats_forward.cpp

#include <format>
#include <iostream>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: stats_forward GAME_NAME [FORWARD_PLY_MAX]";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 1000;

  std::cout << "ply\treachable_states\treachable_positions" << std::endl;
  for(int ply = 0; ply <= forward_ply_max; ++ply)
    {
      std::string forward_name = std::format("forward,ply={:03d}", ply);
      shared_dfa_ptr forward_dfa = game->load_by_name(forward_name);
      if(!forward_dfa)
      {
	std::cerr << "DFA " << forward_name << " not found." << std::endl;
	return 1;
      }

      std::cout << ply << "\t" << forward_dfa->states() << "\t" << forward_dfa->size() << std::endl;
      if(forward_dfa->size() == 0)
	{
	  break;
	}
    }

  return 0;
}
