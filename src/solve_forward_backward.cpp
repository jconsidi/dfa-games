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
      std::cerr << "usage: solve_forward_backward test_forward GAME_NAME [FORWARD_PLY]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 10;
  int backward_ply_max = 0;

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
  
  for(int ply = forward_ply_max; ply >= 0; --ply)
    {
      std::cout << "PLY " << ply << std::endl;
      
      int side_to_move = ply % 2;

      shared_dfa_ptr positions = game->get_positions_forward(ply);

      winning_by_ply[ply] = game->load_or_build(get_name(forward_ply_max, backward_ply_max, ply, "winning"), [&]()
      {
	shared_dfa_ptr won = game->get_positions_won(side_to_move);
	shared_dfa_ptr will_win = game->get_moves_backward(side_to_move, losing_by_ply[ply + 1]);

	return DFAUtil::get_intersection(DFAUtil::get_union(won, will_win),
					 positions);
      });

      losing_by_ply[ply] = game->load_or_build(get_name(forward_ply_max, backward_ply_max, ply, "losing"), [&]()
      {
	shared_dfa_ptr lost = game->get_positions_lost(side_to_move);
	shared_dfa_ptr lost_check = DFAUtil::get_intersection(lost, winning_by_ply[ply]);
	std::cout << "LOST CHECK " << DFAUtil::quick_stats(lost_check) << std::endl;
	assert(lost_check->is_constant(false));
      
	shared_dfa_ptr could_lose = game->get_moves_backward(side_to_move, winning_by_ply[ply + 1]);
	shared_dfa_ptr wont_lose = game->get_moves_backward(side_to_move, DFAUtil::get_inverse(winning_by_ply[ply + 1]));
	shared_dfa_ptr will_lose = DFAUtil::get_difference(could_lose, wont_lose);
	assert(DFAUtil::get_intersection(will_lose, winning_by_ply[ply])->is_constant(false));

	return DFAUtil::get_intersection(DFAUtil::get_union(lost, will_lose),
					 positions);
      });

      unknown_by_ply[ply] = game->load_or_build(get_name(forward_ply_max, backward_ply_max, ply, "unknown"), [&]()
      {
	shared_dfa_ptr winning_or_losing = DFAUtil::get_union(winning_by_ply[ply], losing_by_ply[ply]);
	return DFAUtil::get_difference(positions, winning_or_losing);
      });

      double positions_size = positions->size();
      double winning_size = winning_by_ply[ply]->size();
      double losing_size = losing_by_ply[ply]->size();
      double unknown_size = unknown_by_ply[ply]->size();

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

      shared_dfa_ptr winning_and_losing = DFAUtil::get_intersection(winning_by_ply[ply], losing_by_ply[ply]);
      std::cout << "winning and losing " << DFAUtil::quick_stats(winning_and_losing) << std::endl;
      assert(winning_and_losing->is_constant(false));
}

  return 0;
}
