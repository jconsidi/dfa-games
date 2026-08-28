// verify_backward_sound.cpp

#include <format>
#include <iostream>

#include "test_utils.h"
#include "verify_utils.h"

std::string get_losing_name(int side_to_move, int ply)
{
  return std::format("backward,ply_max={:03d},side={:d},losing", ply, side_to_move);
}

std::string get_winning_name(int side_to_move, int ply)
{
  return std::format("backward,ply_max={:03d},side={:d},winning", ply, side_to_move);
}

void verify_backward_sound(const Game& game, int ply_max)
{
  // ply 0

  for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
    {
      std::string lost_name = get_losing_name(side_to_move, 0);
      verify_lost_sound(game, side_to_move, lost_name);

      std::string won_name = get_winning_name(side_to_move, 0);
      verify_won_sound(game, side_to_move, won_name);
    }

  // later ply
  for(int ply = 1; ply <= ply_max; ++ply)
    {
      for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
        {
          std::string losing_curr_name = get_losing_name(side_to_move, ply);
          std::string winning_prev_name = get_winning_name(1 - side_to_move, ply - 1);
          verify_losing_sound(game, side_to_move, losing_curr_name, winning_prev_name);

          std::string winning_curr_name = get_winning_name(side_to_move, ply);
          std::string losing_prev_name = get_losing_name(1 - side_to_move, ply - 1);
          verify_winning_sound(game, side_to_move, winning_curr_name, losing_prev_name);
        }
    }
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: verify_backward_sound GAME PLY_MAX" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 1;

  verify_backward_sound(*game, ply_max);

  return 0;
}
