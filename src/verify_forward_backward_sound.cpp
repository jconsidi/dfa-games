// verify_forward_backward_sound.cpp

#include <format>
#include <iostream>

#include "test_utils.h"
#include "verify_utils.h"

std::string get_name(int forward_ply_max, int backward_ply_max, int ply, std::string outcome)
{
  return std::format("forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d},{:s}",
                     forward_ply_max, backward_ply_max, ply, outcome);
}

void verify_forward_backward_sound(const Game& game, int forward_ply_max, int backward_ply_max)
{
  auto get_losing_name = [&](int ply)
  {
    return get_name(forward_ply_max, backward_ply_max, ply, "losing");
  };

  auto get_winning_name =[&](int ply)
  {
    return get_name(forward_ply_max, backward_ply_max, ply, "winning");
  };

  int last_side_to_move = forward_ply_max % 2;
  verify_lost_sound(game, last_side_to_move, get_losing_name(forward_ply_max));
  verify_won_sound(game, last_side_to_move, get_winning_name(forward_ply_max));

  for(int ply = forward_ply_max - 1; ply >= 0; --ply)
    {
      int side_to_move = ply % 2;

      std::string losing_curr_name = get_losing_name(ply);
      std::string winning_next_name = get_winning_name(ply + 1);
      verify_losing_sound(game, side_to_move, losing_curr_name, winning_next_name);

      std::string winning_curr_name = get_winning_name(ply);
      std::string losing_next_name = get_losing_name(ply + 1);
      verify_winning_sound(game, side_to_move, winning_curr_name, losing_next_name);
    }
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: verify_forward_backward_sound GAME FORWARD_PLY_MAX BACKWARD_PLY_MAX" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 1;
  int backward_ply_max = (argc >= 4) ? atoi(argv[3]) : 0;

  if(backward_ply_max > 0)
    {
      std::cerr << "BACKWARD_PLY_MAX > 0 not supported yet." << std::endl;
      return 1;
    }

  verify_forward_backward_sound(*game, forward_ply_max, backward_ply_max);

  return 0;
}
