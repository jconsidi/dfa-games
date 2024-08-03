// validate_backward.cpp

#include <iostream>

#include "DFAUtil.h"
#include "Game.h"
#include "test_utils.h"
#include "validate_utils.h"

//static std::format_string<int&, int&, std::string&> backward_format("backward,ply_max={:03d},side_to_move={:d},{:s}");

bool validate_backward(const Game& game, int ply_max, int side_to_move, int max_examples)
{
  std::cout << "############################################################" << std::endl;
  std::cout << "# PLY_MAX = " << ply_max << ", SIDE TO MOVE = " << side_to_move << std::endl;
  std::cout << "############################################################" << std::endl;

  shared_dfa_ptr accept = DFAUtil::get_accept(game.get_shape());

  shared_dfa_ptr curr_losing = game.get_positions_losing(side_to_move, ply_max);
  shared_dfa_ptr curr_winning = game.get_positions_winning(side_to_move, ply_max);
  shared_dfa_ptr curr_unknown = game.get_positions_unknown(side_to_move, ply_max);

  std::cout << "# CHECK PARTITION" << std::endl;

  if(!validate_partition(game, accept, std::vector<shared_dfa_ptr>({curr_winning, curr_losing, curr_unknown})))
    {
      std::cerr << "# PARTITION CHECK FAILED" << std::endl;
      return false;
    }

  if(ply_max == 0)
    {
      // base case - confirm match to lost/won definitions

      std::cout << "# CHECK LOSING = LOST" << std::endl;

      shared_dfa_ptr lost = game.get_positions_lost(side_to_move);
      if(!validate_equal(game, "LOSING", curr_losing, "LOST", lost))
	{
	  std::cerr << "# CHECK LOSING = LOST FAILED" << std::endl;
	  return false;
	}

      std::cout << "# CHECK WINNING = WON" << std::endl;

      shared_dfa_ptr won = game.get_positions_won(side_to_move);
      if(!validate_equal(game, "WINNING", curr_winning, "WON", won))
	{
	  std::cerr << "# CHECK WINNING = WON FAILED" << std::endl;
	  return false;
	}

      return true;
    }

  // recursive cases

  std::cout << "# CHECK LOSING" << std::endl;

  shared_dfa_ptr next_winning = game.get_positions_winning(1 - side_to_move, ply_max - 1);
  shared_dfa_ptr base_losing = game.get_positions_losing(side_to_move, ply_max - 1);

  if(!validate_losing(game, side_to_move, curr_losing, next_winning, base_losing, max_examples))
    {
      return false;
    }

  std::cout << "# CHECK WINNING" << std::endl;

  shared_dfa_ptr next_losing = game.get_positions_losing(1 - side_to_move, ply_max - 1);
  shared_dfa_ptr base_winning = game.get_positions_winning(side_to_move, ply_max - 1);

  if(!validate_winning(game, side_to_move, curr_winning, next_losing, base_winning, max_examples))
    {
      return false;
    }

  std::cout << "# CHECK UNKNOWN" << std::endl;

  shared_dfa_ptr base_unknown = game.get_positions_unknown(side_to_move, ply_max - 1);
  if(!validate_subset(curr_unknown, base_unknown))
    {
      std::cerr << "# UNKNOWN SUBSET CHECK FAILED" << std::endl;
      return false;
    }

  return true;
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: validate_backward GAME_NAME [PLY_MAX]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max =
    (argc >= 3)
    ? atoi(argv[2])
    : 10;

  int max_examples =
    (argc >= 4)
    ? atoi(argv[3])
    : 1000000;

  // run same checks for both sides to move
  for(int ply = 0; ply <= ply_max; ++ply)
    {
      for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
	{
	  if(!validate_backward(*game, ply, side_to_move, max_examples))
	    {
	      return 1;
	    }
	}
    }
  
  return 0;
}
