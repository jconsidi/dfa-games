// Game.cpp

#include "Game.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "DFAUtil.h"
#include "DNFBuilder.h"
#include "IntersectionManager.h"
#include "Profile.h"

Game::Game(std::string name_in, const dfa_shape_t& shape_in)
  : name(name_in),
    shape(shape_in)
{
}

Game::~Game()
{
}

void Game::build_move_graphs(int side_to_move) const
{
  Profile profile("build_move_graphs");

  assert((0 <= side_to_move) && (side_to_move < 2));

  if(move_graphs_ready[side_to_move])
    {
      return;
    }

  move_graphs_forward[side_to_move] = build_move_graph(side_to_move);
  move_graphs_backward[side_to_move] = move_graphs_forward[side_to_move].reverse();
  move_graphs_ready[side_to_move] = true;
}

shared_dfa_ptr Game::get_has_moves(int side_to_move) const
{
  Profile profile("get_has_moves");

  if(!(this->singleton_has_moves[side_to_move]))
    {
      this->singleton_has_moves[side_to_move] = \
	load_or_build("has_moves-side=" + std::to_string(side_to_move),
		      [=]()
		      {
			shared_dfa_ptr all_positions = DFAUtil::get_accept(shape);
			return this->get_moves_backward(side_to_move, all_positions);
		      });
    }

  return this->singleton_has_moves[side_to_move];
}

shared_dfa_ptr Game::get_moves_backward(int side_to_move, shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_backward");

  build_move_graphs(side_to_move);
  return move_graphs_backward[side_to_move].get_moves(positions_in);
}

shared_dfa_ptr Game::get_moves_forward(int side_to_move, shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_forward");

  build_move_graphs(side_to_move);
  return move_graphs_forward[side_to_move].get_moves(positions_in);
}

shared_dfa_ptr Game::get_positions_losing(int side_to_move, int ply_max) const
{
  Profile profile("get_positions_losing");

  assert(ply_max >= 0);

  std::ostringstream dfa_name_builder;
  dfa_name_builder << "losing-side=" << side_to_move << ",ply_max=" << ply_max;

  return load_or_build(dfa_name_builder.str(),
		       [=]()
		       {
			 if(ply_max == 0)
			   {
			     return get_positions_lost(side_to_move);
			   }

			 assert(!"implemented");
		       });
}

shared_dfa_ptr Game::get_positions_winning(int side_to_move, int ply_max) const
{
  Profile profile("get_positions_winning");

  int side_not_to_move = 1 - side_to_move;

  profile.tic("opponent_has_move");
  shared_dfa_ptr opponent_has_move = this->get_has_moves(side_not_to_move);

  profile.tic("lost");
  shared_dfa_ptr lost = this->get_positions_lost(side_not_to_move);
  shared_dfa_ptr losing = lost;

  profile.tic("winning 0");
  shared_dfa_ptr winning = this->get_moves_backward(side_to_move, losing);
  std::cout << "  move 0: " << winning->size() << " winning positions, " << winning->states() << " states" << std::endl;

  for(int move = 1; move < ply_max; ++move)
    {
      auto tic = [&](std::string label_in){profile.tic(label_in + " " + std::to_string(move));};

      tic("not_yet_winning");
      shared_dfa_ptr not_yet_winning = DFAUtil::get_inverse(winning);

      tic("not_yet_losing");
      shared_dfa_ptr not_yet_losing(this->get_moves_backward(side_not_to_move, not_yet_winning));

      tic("losing_more_if_has_move");
      shared_dfa_ptr losing_more_if_has_move = DFAUtil::get_inverse(not_yet_losing);

      tic("losing_more");
      shared_dfa_ptr losing_more = DFAUtil::get_intersection(losing_more_if_has_move, opponent_has_move);

      tic("finished check");
      if(DFAUtil::get_difference(losing_more, losing)->is_constant(false))
	{
	  break;
	}

      tic("losing");
      losing = DFAUtil::get_union(losing, losing_more);

      tic("winning");
      winning = this->get_moves_backward(side_to_move, losing);
      std::cout << "  move " << move << ": " << winning->size() << " winning positions, " << winning->states() << " states" << std::endl;
    }

  return winning;
}

const dfa_shape_t& Game::get_shape() const
{
  return shape;
}

shared_dfa_ptr Game::load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr ()> build_func) const
{
  Profile profile("load_or_build " + dfa_name_in);

  std::string dfa_name = name + "/" + dfa_name_in;

  profile.tic("load");
  try
    {
      shared_dfa_ptr output = shared_dfa_ptr(new DFA(shape, dfa_name));
      output->set_name("saved(\"" + dfa_name_in + "\")");
      std::cout << "loaded " << dfa_name << " => " << DFAUtil::quick_stats(output) << std::endl;

      return output;
    }
  catch(std::runtime_error e)
    {
    }

  profile.tic("build");
  std::cout << "building " << dfa_name << std::endl;
  shared_dfa_ptr output = build_func();
  output->set_name("saved(\"" + dfa_name_in + "\")");

  profile.tic("stats");
  std::cout << "built " << dfa_name << " => " << output->size() << " positions, " << output->states() << " states" << std::endl;

  profile.tic("save");
  output->save(dfa_name);
  return output;
}
