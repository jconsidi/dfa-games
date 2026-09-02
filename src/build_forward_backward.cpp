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

  std::vector<shared_dfa_ptr> losing_by_ply(forward_ply_max + 2, 0);
  std::vector<shared_dfa_ptr> winning_by_ply(forward_ply_max + 2, 0);
  std::vector<shared_dfa_ptr> unknown_by_ply(forward_ply_max + 2, 0);

  shared_dfa_ptr reject = DFAUtil::get_reject(game->get_shape());;

  losing_by_ply[forward_ply_max + 1] = reject;
  winning_by_ply[forward_ply_max + 1] = reject;
  unknown_by_ply[forward_ply_max + 1] = DFAUtil::get_accept(game->get_shape());

  for(int ply = forward_ply_max; ply >= 0; --ply)
    {
      std::cout << "PLY " << ply << std::endl;

      int side_to_move = ply % 2;

      shared_dfa_ptr positions = game->get_positions_forward(ply);
      assert(positions);
      if(positions->is_constant(false))
	{
	  assert(ply == forward_ply_max);

	  // no positions, so trivial solution
	  losing_by_ply[forward_ply_max] = reject;
	  winning_by_ply[forward_ply_max] = reject;
	  unknown_by_ply[forward_ply_max] = reject;

	  --forward_ply_max;
	  continue;
	}

      assert(losing_by_ply[ply + 1]);
      assert(winning_by_ply[ply + 1]);
      assert(unknown_by_ply[ply + 1]);

      double positions_size = positions->size();
      std::cout << "PLY " << ply << " POSITIONS " << positions_size << std::endl;

      // after first intersecting with the backward_ply_max solution,
      // then we can just use the terminal case for speed. any use of
      // backward_ply_max at the current ply is only
      // backward_ply_max-1 at the next ply, so we would get it
      // through the next DFAs.

      shared_dfa_ptr winning_base =
        (ply == forward_ply_max)
        ? game->get_positions_winning(side_to_move, backward_ply_max)
        : game->get_positions_won(side_to_move);

      shared_dfa_ptr losing_base =
        (ply == forward_ply_max)
        ? game->get_positions_losing(side_to_move, backward_ply_max)
        : game->get_positions_lost(side_to_move);

      build_triple built =
        build_backward(*game,
                       side_to_move,
                       positions,
                       get_name(forward_ply_max, backward_ply_max, ply, "winning"),
                       get_name(forward_ply_max, backward_ply_max, ply, "losing"),
                       get_name(forward_ply_max, backward_ply_max, ply, "unknown"),
                       winning_by_ply[ply + 1],
                       losing_by_ply[ply + 1],
                       unknown_by_ply[ply + 1],
                       winning_base,
                       losing_base);

      winning_by_ply[ply] = std::get<0>(built);
      losing_by_ply[ply] = std::get<1>(built);
      unknown_by_ply[ply] = std::get<2>(built);

      double winning_size = winning_by_ply[ply]->size();
      std::cout << "PLY " << ply << " WINNING " << winning_size << std::endl;

      double losing_size = losing_by_ply[ply]->size();
      std::cout << "PLY " << ply << " LOSING " << losing_size << std::endl;

      double unknown_size = unknown_by_ply[ply]->size();
      std::cout << "PLY " << ply << " UNKNOWN " << unknown_size << std::endl;

      auto summarize_result = [&](std::string result, double result_size)
      {
	std::ostringstream output;
	output << std::defaultfloat << result_size << " " << result << " (" << std::setprecision(4) << std::fixed << (result_size / positions_size) << ")";
	return output.str();
      };

      std::cout << "PLY " << ply << " STATS " << std::defaultfloat << positions_size << " positions, ";
      std::cout << summarize_result("winning", winning_size) << ", ";
      std::cout << summarize_result("losing", losing_size) << ", ";
      std::cout << summarize_result("unknown", unknown_size) << std::endl;

#ifdef PARANOIA
      shared_dfa_ptr winning_and_losing = DFAUtil::get_intersection(winning_by_ply[ply], losing_by_ply[ply]);
      assert(winning_and_losing->is_constant(false));
#endif
    }

  return 0;
}
