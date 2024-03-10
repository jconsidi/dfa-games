// validate_forward_backward.cpp

#include <cstdlib>
#include <format>
#include <iostream>
#include <utility>

#include "DFAUtil.h"
#include "Game.h"
#include "test_utils.h"
#include "validate_utils.h"

static std::format_string<int&> positions_format("forward,ply={:03d}");
static std::format_string<int&, int&, int&> winning_format("forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d},winning");
static std::format_string<int&, int&, int&> losing_format("forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d},losing");

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: validate_forward_backward test_forward GAME_NAME [FORWARD_PLY] [BACKWARD_PLY] [MAX_EXAMPLES]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 10;
  int backward_ply_max = (argc >= 4) ? atoi(argv[3]) : 0;
  int max_examples = (argc >= 5) ? atoi(argv[4]) : 1000000;

  dfa_shape_t shape = game->get_shape();

  for(int ply = forward_ply_max; ply >= 0; --ply)
    {
      std::cout << "############################################################" << std::endl;
      std::cout << "PLY " << ply << std::endl;
      std::cout << "############################################################" << std::endl;

      int side_to_move = ply % 2;

      shared_dfa_ptr positions = game->get_positions_forward_bound(ply);
      if(!positions)
        {
          positions = load_helper(*game, positions_format, ply);
        }
      if(!positions)
	{
	  return 1;
	}

      shared_dfa_ptr winning = load_helper(*game, winning_format, forward_ply_max, backward_ply_max, ply);
      if(!winning)
	{
	  return 1;
	}

      shared_dfa_ptr losing = load_helper(*game, losing_format, forward_ply_max, backward_ply_max, ply);
      if(!losing)
	{
	  return 1;
	}

      std::format_string<int&, int&, int&> unknown_format("forward_backward,forward_ply_max={:03d},backward_ply_max={:03d},ply={:03d},unknown");
      shared_dfa_ptr unknown = load_helper(*game, unknown_format, forward_ply_max, backward_ply_max, ply);
      if(!unknown)
	{
	  return 1;
	}

      std::cout << "PARTITION CHECK" << std::endl;

      if(!validate_partition(*game, positions, std::vector<shared_dfa_ptr>({winning, losing, unknown})))
	{
	  std::cerr << "PARTITION CHECK FAILED" << std::endl;
	  return 1;
	}

      std::cout << "BACKWARD WINNING CHECK" << std::endl;

      shared_dfa_ptr backward_winning = game->get_positions_winning(side_to_move, backward_ply_max);

      shared_dfa_ptr backward_winning_positions = DFAUtil::get_intersection(backward_winning, positions);
      if(!validate_subset(backward_winning_positions, winning))
	{
	  std::cerr << "BACKWARD WINNING CHECK FAILED" << std::endl;
	  return 1;
	}

      std::cout << "BACKWARD LOSING CHECK" << std::endl;

      shared_dfa_ptr backward_losing = game->get_positions_losing(side_to_move, backward_ply_max);

      shared_dfa_ptr backward_losing_positions = DFAUtil::get_intersection(backward_losing, positions);
      if(!validate_subset(backward_losing_positions, losing))
	{
	  std::cerr << "BACKWARD LOSING CHECK FAILED" << std::endl;
	  return 1;
	}

      std::cout << "WINNING CHECK" << std::endl;

      int next_ply = ply + 1;
      shared_dfa_ptr next_losing =
	(ply < forward_ply_max)
	? load_helper(*game, losing_format, forward_ply_max, backward_ply_max, next_ply)
	: DFAUtil::get_reject(shape);

      if(!validate_winning(*game, side_to_move, winning, next_losing, backward_winning, max_examples))
	{
	  return 1;
	}

      std::cout << "LOSING CHECK" << std::endl;

      shared_dfa_ptr next_winning =
	(ply < forward_ply_max)
	? load_helper(*game, winning_format, forward_ply_max, backward_ply_max, next_ply)
	: DFAUtil::get_reject(shape);

      if(!validate_losing(*game, side_to_move, losing, next_winning, backward_losing, max_examples))
	{
	  return 1;
	}
    }

  return 0;
}
