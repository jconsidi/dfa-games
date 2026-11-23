// verify_forward_exact.cpp

// Verifies a forward solve's sets are exactly the reachable positions:
// forward,ply=000 is the initial position alone, and each forward,ply=NNN
// below PLY_MAX is followed by exactly the positions its moves reach (see
// verify_forward_exact in verify_utils.h). Memory is one bit per position
// of the next ply plus a count per DFA state.

#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "game_utils.h"
#include "verify_utils.h"

int main(int argc, char **argv)
{
  if(argc != 3)
    {
      std::cerr << "usage: verify_forward_exact GAME PLY_MAX" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = atoi(argv[2]);
  if(ply_max < 0)
    {
      std::cerr << "PLY_MAX must be at least 0." << std::endl;
      return 1;
    }

  auto name = [](int ply)
  {
    return std::format("forward,ply={:03d}", ply);
  };

  shared_dfa_ptr curr = verify_load(*game, name(0));
  DFAString initial = game->get_position_initial();
  if((curr->size() != 1) || !curr->contains(initial))
    {
      throw std::runtime_error(name(0) + " has " + std::to_string(curr->size()) + " positions and is not exactly the initial position");
    }
  std::cout << "VERIFIED " << name(0) << " IS EXACTLY THE INITIAL POSITION" << std::endl;

  std::vector<uint64_t> counts = {1};
  for(int ply = 0; ply < ply_max; ++ply)
    {
      shared_dfa_ptr next = verify_load(*game, name(ply + 1));
      std::cout << "PLY " << ply << " SIDE " << (ply % 2) << ": " << name(ply) << " TO " << name(ply + 1) << std::endl;
      counts.push_back(verify_forward_exact(*game, ply % 2, curr, next));
      curr = next;
    }

  std::cout << "VERIFIED FORWARD PLIES 0-" << ply_max << " ARE EXACTLY THE REACHABLE POSITIONS" << std::endl;
  std::cout << std::format("{:>4} {:>16}", "ply", "positions") << std::endl;
  for(size_t ply = 0; ply < counts.size(); ++ply)
    {
      std::cout << std::format("{:>4} {:>16}", ply, counts[ply]) << std::endl;
    }

  return 0;
}
