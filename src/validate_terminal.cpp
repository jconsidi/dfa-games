// validate_terminal.cpp

// validate terminal positions, e.g. positions without moves.

#include <iostream>

#include "DFAUtil.h"
#include "Game.h"
#include "test_utils.h"
#include "validate_utils.h"

bool validate_side_to_move(const Game& game, int side_to_move, int max_examples)
{
  std::cout << "############################################################" << std::endl;
  std::cout << "# SIDE TO MOVE: " << side_to_move << std::endl;
  std::cout << "############################################################" << std::endl;
  
  dfa_shape_t shape = game.get_shape();

  shared_dfa_ptr won = game.get_positions_won(side_to_move);
  shared_dfa_ptr lost = game.get_positions_lost(side_to_move);

  std::cout << "# CHECK won/lost defined" << std::endl;
  if(won->is_constant(0) && lost->is_constant(0))
    {
      std::cerr << "# CHECK won/lost defined failed : both are empty" << std::endl;
      return false;
    }

  std::cout << "# CHECK won/lost disjoint" << std::endl;
  if(!validate_disjoint(won, lost))
    {
      std::cerr << "# CHECK won/lost disjoint failed" << std::endl;
      return false;
    }

  std::cout << "# CHECK won" << std::endl;
  if(!validate_result(game, side_to_move, won, 1, max_examples))
    {
      std::cerr << "# CHECK won failed" << std::endl;
      return false;
    }

  std::cout << "# CHECK lost" << std::endl;
  if(!validate_result(game, side_to_move, lost, -1, max_examples))
    {
      std::cerr << "# CHECK lost failed" << std::endl;
      return false;
    }

  std::cout << "# CHECK has_moves" << std::endl;

  shared_dfa_ptr has_moves = game.get_has_moves(side_to_move);
  if(!validate_result(game, side_to_move, has_moves, 0, max_examples))
    {
      std::cerr << "# CHECK has_moves failed" << std::endl;
      return false;
    }

  std::cout << "# CHECK drawn" << std::endl;

  shared_dfa_ptr not_drawn = DFAUtil::get_union_vector(shape,
						       std::vector<shared_dfa_ptr>({won, lost, has_moves}));
  shared_dfa_ptr drawn = DFAUtil::get_inverse(not_drawn);
  if(!validate_result(game, side_to_move, drawn, 0, max_examples))
    {
      std::cerr << "# CHECK drawn failed" << std::endl;
      return false;
    }

  return true;
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: validate_terminal GAME_NAME [MAX_EXAMPLES]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int max_examples = (argc >= 3) ? atoi(argv[2]) : 1000000;

  // run same checks for both sides to move
  for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
    {
      if(!validate_side_to_move(*game, side_to_move, max_examples))
	{
	  return 1;
	}
    }
  
  return 0;
}
