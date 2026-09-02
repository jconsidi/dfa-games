// solve_forward_backward.cpp

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"
#include "build_utils.h"
#include "test_utils.h"

std::string get_name(int forward_ply_max, int backward_ply_max, int ply, std::string result)
{
  std::ostringstream output;
  output << "forward_backward";
  output << ",forward_ply_max=" << std::setfill('0') << std::setw(3) << forward_ply_max;
  output << ",backward_ply_max=" << std::setfill('0') << std::setw(3) << backward_ply_max;
  output << ",ply=" << std::setfill('0') << std::setw(3) << ply;
  output << "," << result;

  return output.str();
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: solve_forward_backward test_forward GAME_NAME [FORWARD_PLY] [BACKWARD_PLY]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 100;
  int backward_ply_max = (argc >= 4) ? atoi(argv[3]) : 0;

  auto initial_positions = game->get_positions_initial();
  assert(initial_positions->size() == 1);

  std::cout << game->position_to_string(game->get_position_initial()) << std::endl;

  std::vector<shared_dfa_ptr> forward_positions = build_forward(*game, forward_ply_max);
  forward_ply_max = int(forward_positions.size()) - 1;

  std::vector<std::string> winning_names;
  std::vector<std::string> losing_names;
  std::vector<std::string> unknown_names;

  for(int ply = 0; ply <= forward_ply_max; ++ply)
    {
      winning_names.push_back(get_name(forward_ply_max, backward_ply_max, ply, "winning"));
      losing_names.push_back(get_name(forward_ply_max, backward_ply_max, ply, "losing"));
      unknown_names.push_back(get_name(forward_ply_max, backward_ply_max, ply, "unknown"));
    }

  int last_side_to_move = forward_ply_max % 2;
  shared_dfa_ptr winning_backward = game->get_positions_winning(last_side_to_move, backward_ply_max);
  shared_dfa_ptr losing_backward = game->get_positions_losing(last_side_to_move, backward_ply_max);

  build_backward_many(*game,
                      forward_positions,
                      winning_names,
                      losing_names,
                      unknown_names,
                      winning_backward,
                      losing_backward);

  return 0;
}
