// verify_forward_backward.cpp

// Verifies a forward_backward solve with verify_solve (verify_utils.h): each
// ply's domain is forward,ply=NNN, partitioned by the solve's winning,
// losing and unknown sets, with every move from a ply's domain in the next
// ply's domain.

#include <format>
#include <iostream>
#include <string>

#include "game_utils.h"
#include "verify_utils.h"

int main(int argc, char **argv)
{
  if(argc != 4)
    {
      std::cerr << "usage: verify_forward_backward GAME FORWARD_PLY_MAX BACKWARD_PLY_MAX" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = atoi(argv[2]);
  int backward_ply_max = atoi(argv[3]);
  if(backward_ply_max > 0)
    {
      std::cerr << "BACKWARD_PLY_MAX > 0 not supported yet." << std::endl;
      return 1;
    }

  verify_solve(*game, forward_ply_max, [&](int ply)
  {
    std::string prefix = std::format("forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d},",
                                     forward_ply_max, backward_ply_max, ply);
    return VerifyPlySets{verify_load(*game, std::format("forward,ply={:03d}", ply)),
                         verify_load(*game, prefix + "winning"),
                         verify_load(*game, prefix + "losing"),
                         verify_load(*game, prefix + "unknown")};
  }, VerifyClosure::every_move);

  return 0;
}
