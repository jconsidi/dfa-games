// solve_forward_backward.cpp

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"
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

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 10;
  int backward_ply_max = (argc >= 4) ? atoi(argv[3]) : 0;

  auto initial_positions = game->get_positions_initial();
  assert(initial_positions->size() == 1);

  for(auto iter = initial_positions->cbegin();
      iter < initial_positions->cend();
      ++iter)
    {
      std::cout << game->position_to_string(*iter) << std::endl;
    }

  std::vector<shared_dfa_ptr> losing_by_ply(forward_ply_max + 2, 0);
  std::vector<shared_dfa_ptr> winning_by_ply(forward_ply_max + 2, 0);
  std::vector<shared_dfa_ptr> unknown_by_ply(forward_ply_max + 2, 0);

  losing_by_ply[forward_ply_max + 1] = DFAUtil::get_reject(game->get_shape());
  winning_by_ply[forward_ply_max + 1] = DFAUtil::get_reject(game->get_shape());
  unknown_by_ply[forward_ply_max + 1] = DFAUtil::get_accept(game->get_shape());

  bool complete_expected = false;
  for(int ply = forward_ply_max; ply >= 0; --ply)
    {
      std::cout << "PLY " << ply << std::endl;

      int side_to_move = ply % 2;

      shared_dfa_ptr positions = game->get_positions_forward(ply);
      if(positions->is_constant(false))
	{
	  complete_expected = true;
	}
      double positions_size = positions->size();
      std::cout << "PLY " << ply << " POSITIONS " << positions_size << std::endl;

      winning_by_ply[ply] = game->load_or_build(get_name(forward_ply_max, backward_ply_max, ply, "winning"), [&]()
      {
	shared_dfa_ptr backward_winning = game->get_positions_winning(side_to_move, backward_ply_max);
	shared_dfa_ptr will_win = game->get_moves_backward(side_to_move, losing_by_ply[ply + 1]);

	return DFAUtil::get_intersection(DFAUtil::get_union(backward_winning, will_win),
					 positions);
      });
      double winning_size = winning_by_ply[ply]->size();
      std::cout << "PLY " << ply << " WINNING " << winning_size << std::endl;

#ifndef PARANOIA
      bool next_ply_complete = unknown_by_ply[ply + 1]->is_constant(0);
#endif

      losing_by_ply[ply] = game->load_or_build(get_name(forward_ply_max, backward_ply_max, ply, "losing"), [&]()
      {
#ifndef PARANOIA
	if(next_ply_complete)
	  {
	    // if next ply was completely solved, then we can just do
	    // set subtraction.
	    return DFAUtil::get_difference(positions, winning_by_ply[ply]);
	  }
#endif

	shared_dfa_ptr backward_losing = game->get_positions_losing(side_to_move, backward_ply_max);

	shared_dfa_ptr could_lose = game->get_moves_backward(side_to_move, winning_by_ply[ply + 1]);
	shared_dfa_ptr wont_lose = game->get_moves_backward(side_to_move, DFAUtil::get_inverse(winning_by_ply[ply + 1]));
	shared_dfa_ptr will_lose = DFAUtil::get_difference(could_lose, wont_lose);

	return DFAUtil::get_intersection(DFAUtil::get_union(backward_losing, will_lose),
					 positions);
      });
      double losing_size = losing_by_ply[ply]->size();
      std::cout << "PLY " << ply << " LOSING " << losing_size << std::endl;

      unknown_by_ply[ply] = game->load_or_build(get_name(forward_ply_max, backward_ply_max, ply, "unknown"), [&]()
      {
#ifndef PARANOIA
	if(next_ply_complete)
	  {
	    return DFAUtil::get_reject(game->get_shape());
	  }
#endif

	shared_dfa_ptr winning_or_losing = DFAUtil::get_union(winning_by_ply[ply], losing_by_ply[ply]);
	return DFAUtil::get_difference(positions, winning_or_losing);
      });
      double unknown_size = unknown_by_ply[ply]->size();
      std::cout << "PLY " << ply << " UNKNOWN " << unknown_size << std::endl;
      if(complete_expected)
	{
	  assert(unknown_by_ply[ply]->is_constant(false));
	}

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
