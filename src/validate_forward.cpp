// test_forward.cpp

#include <cstdlib>
#include <iostream>

#include "DFAUtil.h"
#include "test_utils.h"
#include "validate_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_forward GAME_NAME [depth] [max examples]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 2;
  int max_examples = (argc >= 4) ? atoi(argv[3]) : 1000000;

  dfa_shape_t shape = game->get_shape();

  DFAString initial_position = game->get_position_initial();
  std::cout << "# INITIAL POSITION" << std::endl;
  std::cout << game->position_to_string(initial_position) << std::endl;

  bool expected_complete = true;
  std::vector<DFAString> expected_samples = {initial_position};

  for(int ply = 0; ply <= ply_max; ++ply)
    {
      std::cout << "############################################################" << std::endl;
      std::cout << "# PLY " << ply << std::endl;
      std::cout << "############################################################" << std::endl;

      shared_dfa_ptr positions = game->get_positions_forward(ply);
      std::cout << std::endl;
      if(ply == 0)
	{
	  assert(positions->size() == 1);
	}

      std::cout << "# CHECKING INCLUSION" << std::endl;

      for(const DFAString& expected_sample : expected_samples)
	{
	  if(!positions->contains(expected_sample))
	    {
	      std::cerr << "# MISSING POSITION AT PLY " << ply << std::endl;
	      std::cerr << game->position_to_string(expected_sample) << std::endl;
	      return 1;
	    }
	}

      // if the sample is supposed to be complete, make sure no extra
      // positions were generated.

      if(expected_complete)
	{
	  std::cout << "# CHECKING EQUALITY" << std::endl;

	  shared_dfa_ptr expected_u =
	    (expected_samples.size() > 0)
	    ? DFAUtil::from_strings(shape, expected_samples)
	    : DFAUtil::get_reject(shape);

	  if(!validate_equal(*game, "ACTUAL", positions, "EXPECTED", expected_u))
	    {
	      std::cerr << "# POSITIONS DIFFERENCE AT PLY " << ply << std::endl;
	      return 1;
	    }
	}
      else
	{
	  std::cout << "# SKIPPING EQUALITY (incomplete samples)" << std::endl;
	}

      // use lexicographically first positions to get a (non-random)
      // sample of positions expected after next ply.

      std::cout << "# CHECKING FIRST " << max_examples << " POSITIONS" << std::endl;

      int side_to_move = ply % 2;

      expected_complete = max_examples >= positions->size();

      expected_samples.clear();
      int k = 0;
      for(auto iter = positions->cbegin();
	  (iter < positions->cend()) && (k < max_examples);
	  ++iter, ++k)
	{
	  DFAString position(*iter);

	  std::vector<DFAString> moves = game->validate_moves(side_to_move, position);
          std::optional<int> result = game->validate_result(side_to_move, position);

	  // should not have both moves and a final result
	  if((moves.size() > 0) && result)
	    {
	      std::cerr << "# CONFLICT" << std::endl;
	      std::cerr << game->position_to_string(position) << std::endl;
	      std::cerr << "# position has " << moves.size() << " moves and final result " << *result << std::endl;
	      return 1;
	    }

	  for(const DFAString& move : moves)
	    {
	      expected_samples.push_back(move);
	    }
	}
    }

  return 0;
}
