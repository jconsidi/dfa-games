// verify_backward.cpp

// Verifies a backward solve with verify_solve (verify_utils.h). The sets
// backward,ply_max=K,side=S,{winning,losing,unknown} each partition every
// position of the game's shape, and are checked against
// backward,ply_max=K-1,side=1-S. That gives two chains from PLY_MAX down to
// ply_max 0, one starting from each side, and together they cover both
// sides at every ply_max. In verify_solve's terms, ply p of a chain is
// ply_max PLY_MAX - p, so ply_max 0 is its last ply, where winning
// positions must be won, losing positions lost, and unknown positions tied
// or not over.
//
// The domain is every position of the shape, so this only suits small
// games.

#include <format>
#include <iostream>
#include <string>

#include "DFAUtil.h"
#include "game_utils.h"
#include "verify_utils.h"

int main(int argc, char **argv)
{
  if(argc != 3)
    {
      std::cerr << "usage: verify_backward GAME PLY_MAX" << std::endl;
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

  shared_dfa_ptr all_positions = DFAUtil::get_accept(game->get_shape());

  for(int first_side = 0; first_side < 2; ++first_side)
    {
      std::cout << "CHAIN FROM SIDE " << first_side << " AT PLY_MAX " << ply_max << std::endl;

      verify_solve(*game, ply_max, first_side, [&](int ply)
      {
        int backward_ply_max = ply_max - ply;
        int side_to_move = (first_side + ply) % 2;
        std::string prefix = std::format("backward,ply_max={:03d},side={:d},", backward_ply_max, side_to_move);
        return VerifyPlySets{all_positions,
                             verify_load(*game, prefix + "winning"),
                             verify_load(*game, prefix + "losing"),
                             verify_load(*game, prefix + "unknown")};
      }, VerifyClosure::every_move);
    }

  std::cout << "VERIFIED BACKWARD SOLVE " << game_name << " PLY_MAX 0-" << ply_max << " FOR BOTH SIDES" << std::endl;

  return 0;
}
