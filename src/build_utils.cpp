// build_utils.cpp

#include "build_utils.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "DFAUtil.h"

void build_backward_many(const Game& game,
                         std::vector<shared_dfa_ptr> targets,
                         std::vector<std::string> winning_names,
                         std::vector<std::string> losing_names,
                         std::vector<std::string> unknown_names,
                         shared_dfa_ptr winning_base,
                         shared_dfa_ptr losing_base)
{
  assert(targets.size() > 0);
  assert(winning_names.size() == targets.size());
  assert(losing_names.size() == targets.size());
  assert(unknown_names.size() == targets.size());

  auto log_ply_begin = [&](int ply)
  {
    double target_size = targets.at(ply)->size();

    std::cout << "PLY " << ply << " POSITIONS " << target_size << std::endl;
  };

  // last ply: just intersect with base cases

  // last targets typically have no moves.

  int last_ply = int(targets.size()) - 1;
  log_ply_begin(last_ply);

  shared_dfa_ptr winning_next =
    game.load_or_build(winning_names[last_ply], [&]()
    {
      return DFAUtil::get_intersection(targets[last_ply], winning_base);
    });

  shared_dfa_ptr losing_next =
    game.load_or_build(losing_names[last_ply], [&]()
    {
      return DFAUtil::get_intersection(targets[last_ply], losing_base);
    });

  shared_dfa_ptr unknown_next =
    game.load_or_build(unknown_names[last_ply], [&]()
    {
      return build_unknown(targets[last_ply], winning_next, losing_next);
    });

  auto log_ply_end = [&](int ply)
  {
    double target_size = targets.at(ply)->size();

    double winning_size = winning_next->size();
    std::cout << "PLY " << ply << " WINNING " << winning_size << std::endl;

    double losing_size = losing_next->size();
    std::cout << "PLY " << ply << " LOSING " << losing_size << std::endl;

    double unknown_size = unknown_next->size();
    std::cout << "PLY " << ply << " UNKNOWN " << unknown_size << std::endl;

    auto summarize_result = [&](std::string result, double result_size)
    {
      std::ostringstream output;
      output << std::defaultfloat << result_size << " " << result << " (" << std::setprecision(4) << std::fixed << (result_size / target_size) << ")";
      return output.str();
    };

    std::cout << "PLY " << ply << " STATS " << std::defaultfloat << target_size << " positions, ";
    std::cout << summarize_result("winning", winning_size) << ", ";
    std::cout << summarize_result("losing", losing_size) << ", ";
    std::cout << summarize_result("unknown", unknown_size) << std::endl;
  };

  // backup to beginning

  for(int ply = int(targets.size()) - 2; ply >= 0; --ply)
    {
      log_ply_begin(ply);

      build_triple built =
        build_backward_once(game,
                            ply % 2,
                            targets[ply],
                            winning_names[ply],
                            losing_names[ply],
                            unknown_names[ply],
                            winning_next,
                            losing_next,
                            unknown_next);

      winning_next = std::get<0>(built);
      losing_next = std::get<1>(built);
      unknown_next = std::get<2>(built);

      log_ply_end(ply);
    }
}

build_triple build_backward_once(const Game& game,
                                 int side_to_move,
                                 shared_dfa_ptr target_curr,
                                 std::string winning_curr_name,
                                 std::string losing_curr_name,
                                 std::string unknown_curr_name,
                                 shared_dfa_ptr winning_next,
                                 shared_dfa_ptr losing_next,
                                 shared_dfa_ptr unknown_next)
{
  shared_dfa_ptr winning_curr =
    game.load_or_build(winning_curr_name, [&]()
    {
      shared_dfa_ptr won = game.get_positions_won(side_to_move);
      shared_dfa_ptr will_win = game.get_moves_backward(side_to_move, losing_next);

      return DFAUtil::get_intersection(DFAUtil::get_union(won, will_win),
                                       target_curr);
    });

  bool next_complete = unknown_next->is_constant(0);
  
  shared_dfa_ptr losing_curr =
    game.load_or_build(losing_curr_name, [&]()
    {
      if(next_complete)
        {
          // if next ply was completely solved, then we can just do
          // set subtraction.
          return DFAUtil::get_difference(target_curr, winning_curr);
        }

      shared_dfa_ptr lost = game.get_positions_lost(side_to_move);

      shared_dfa_ptr could_lose = game.get_moves_backward(side_to_move, winning_next);
      shared_dfa_ptr wont_lose = game.get_moves_backward(side_to_move, DFAUtil::get_inverse(winning_next));
      // could_lose requirement is to make sure that they are forced
      // to move to a position where the other side is winning.
      shared_dfa_ptr will_lose = DFAUtil::get_difference(could_lose, wont_lose);

      // combine with "shorter" losses. usually terminal lost or the
      // pure backward losing positions.
      return DFAUtil::get_intersection(DFAUtil::get_union(lost, will_lose),
                                       target_curr);
    });

  shared_dfa_ptr unknown_curr =
    game.load_or_build(unknown_curr_name, [&]()
    {
      if(next_complete)
        {
          return DFAUtil::get_reject(game.get_shape());
        }

      return build_unknown(target_curr, winning_curr, losing_curr);
    });

  if(next_complete)
    {
      // current layer should also be complete
      assert(unknown_curr->is_constant(false));
    }

  return build_triple(winning_curr, losing_curr, unknown_curr);
}

std::vector<shared_dfa_ptr> build_forward(const Game& game, int forward_ply_max)
{
  std::vector<shared_dfa_ptr> output;
  output.push_back(game.get_positions_initial());

  for(int ply = 1; ply <= forward_ply_max; ++ply)
    {
      shared_dfa_ptr positions = game.get_positions_forward(ply);
      std::cout << positions->size() << " positions after " << ply << " ply." << std::endl;
      if(positions->size() == 0)
	{
	  break;
	}

      output.push_back(positions);
    }

  return output;
}

shared_dfa_ptr build_unknown(shared_dfa_ptr target, shared_dfa_ptr winning, shared_dfa_ptr losing)
{
  shared_dfa_ptr winning_or_losing = DFAUtil::get_union(winning, losing);
  return DFAUtil::get_difference(target, winning_or_losing);
}
