// build_forward_backward_proof.cpp

#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "game_utils.h"
#include "proof_utils.h"

std::string get_name(int forward_ply_max, int backward_ply_max, int ply, std::string result)
{
  return std::format("forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d},{:s}",
                     forward_ply_max, backward_ply_max, ply, result);
}

std::string get_proof_name(int forward_ply_max, int backward_ply_max, int ply)
{
  return std::format("proof_forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d}",
                     forward_ply_max, backward_ply_max, ply);
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: build_forward_backward_proof GAME_NAME [FORWARD_PLY] [BACKWARD_PLY]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 100;
  int backward_ply_max = (argc >= 4) ? atoi(argv[3]) : 0;

  std::vector<std::string> winning_names;
  std::vector<std::string> losing_names;
  std::vector<std::string> proof_names;
  for(int ply = 0; ply <= forward_ply_max; ++ply)
    {
      winning_names.push_back(get_name(forward_ply_max, backward_ply_max, ply, "winning"));
      losing_names.push_back(get_name(forward_ply_max, backward_ply_max, ply, "losing"));
      proof_names.push_back(get_proof_name(forward_ply_max, backward_ply_max, ply));
    }

  build_proof(*game,
              load_proof_inputs(*game, winning_names),
              load_proof_inputs(*game, losing_names),
              proof_names);

  return 0;
}
