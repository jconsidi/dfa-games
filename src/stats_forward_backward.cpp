// stats_forward_backward.cpp

#include <format>
#include <iostream>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: stats_forward_backward test_forward GAME_NAME [FORWARD_PLY] [BACKWARD_PLY]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 10;
  int backward_ply_max = (argc >= 4) ? atoi(argv[3]) : 0;

  auto load_helper = [&](std::string dfa_name) {
    shared_dfa_ptr dfa = game->load_by_name(dfa_name);
    if(!dfa)
      {
	std::cerr << "DFA " << dfa_name << " not found." << std::endl;
      }
    return dfa;
  };

  auto load_result = [&](int ply, std::string result) {
    std::string dfa_name = std::format("forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d},{:s}", forward_ply_max, backward_ply_max, ply, result);
    return load_helper(dfa_name);
  };

  std::cout << "ply\treachable_states\treachable_positions\twinning_states\twinning_positions\tlosing_states\tlosing_positions\tunknown_states\tunknown_positions" << std::endl;
  for(int ply = 0; ply <= forward_ply_max; ++ply)
    {
      std::string reachable_name = std::format("forward,ply={:03d}", ply);
      shared_dfa_ptr reachable = load_helper(reachable_name);
      if(!reachable)
	{
	  return 1;
	}

      shared_dfa_ptr winning = load_result(ply, "winning");
      if(!winning)
	{
	  continue;
	}

      shared_dfa_ptr losing = load_result(ply, "losing");
      if(!losing)
	{
	  continue;
	}

      shared_dfa_ptr unknown = load_result(ply, "unknown");
      if(!unknown)
	{
	  continue;
	}

      std::cout << ply << "\t" << reachable->states() << "\t" << reachable->size() << "\t" << winning->states() << "\t" << winning->size() << "\t" << losing->states() << "\t" << losing->size() << "\t" << unknown->states() << "\t" << unknown->size() << std::endl;
    }

  return 0;
}
