// test_forward.cpp

#include <cstdlib>
#include <iostream>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_forward GAME_NAME [depth]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 2;
  int check_positions = 1000000;

  dfa_shape_t shape = game->get_shape();

  DFAString initial_position = game->get_position_initial();
  std::cout << "INITIAL POSITION" << std::endl;
  std::cout << game->position_to_string(initial_position) << std::endl;

  bool expected_complete = true;
  std::vector<DFAString> expected_samples = {initial_position};

  for(int ply = 0; ply <= ply_max; ++ply)
    {
      shared_dfa_ptr positions = game->get_positions_forward(ply);
      std::cout << positions->size() << " positions after " << ply << " ply." << std::endl;
      std::cout << std::endl;
      if(ply == 0)
	{
	  assert(positions->size() == 1);
	}

      for(const DFAString& expected_sample : expected_samples)
	{
	  if(!positions->contains(expected_sample))
	    {
	      std::cerr << "MISSING POSITION AT PLY " << ply << std::endl;
	      std::cerr << game->position_to_string(expected_sample) << std::endl;
	      return 1;
	    }
	}

      // if the sample is supposed to be complete, make sure no extra
      // positions were generated.

      if(expected_complete)
	{
	  shared_dfa_ptr expected_u =
	    (expected_samples.size() > 0)
	    ? DFAUtil::from_strings(shape, expected_samples)
	    : DFAUtil::get_reject(shape);

	  if(!DFAUtil::check_equal(positions, expected_u))
	    {
	      std::cerr << "POSITIONS DIFFERENCE AT PLY " << ply << std::endl;
	      return 1;
	    }
	}

      // use lexicographically first positions to get a (non-random)
      // sample of positions expected after next ply.

      int side_to_move = ply % 2;

      expected_complete = check_positions >= positions->size();

      expected_samples.clear();
      int k = 0;
      for(auto iter = positions->cbegin();
	  (iter < positions->cend()) && (k < check_positions);
	  ++iter, ++k)
	{
	  DFAString position(*iter);

	  std::vector<DFAString> moves = game->validate_moves(side_to_move, position);
	  int result = game->validate_result(side_to_move, position);

	  // should not have both moves and a final result 
	  if((moves.size() > 0) && (result != 0))
	    {
	      std::cerr << "CONFLICT" << std::endl;
	      std::cerr << game->position_to_string(position) << std::endl;
	      std::cerr << "position has " << moves.size() << " moves and final result " << result << std::endl;
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
