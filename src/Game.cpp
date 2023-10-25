// Game.cpp

#include "Game.h"

#include <sys/stat.h>

#include <algorithm>
#include <iomanip>
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
  std::string directory = std::string("scratch/") + name_in;
  mkdir(directory.c_str(), 0700);
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
		      [&]()
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

std::string Game::get_name() const
{
  return name;
}

shared_dfa_ptr Game::get_positions_initial() const
{
  return DFAUtil::from_string(get_position_initial());
}

shared_dfa_ptr Game::get_positions_reachable(int ply) const
{
  assert(ply >= 0);

  if(ply == 0)
    {
      return get_positions_initial();
    }

  std::string name = "reachable,ply=" + std::to_string(ply);
  return load_or_build(name,
		       [&]()
                       {
			 shared_dfa_ptr previous = get_positions_reachable(ply - 1);
			 return get_moves_forward((ply - 1) % 2, previous);
		       });
}

std::string Game::get_name_losing(int side_to_move, int ply_max) const
{
  std::ostringstream dfa_name_builder;
  dfa_name_builder << "ply_max=" << std::setfill('0') << std::setw(3) << ply_max << ",side=" << side_to_move << ",losing";
  return dfa_name_builder.str();
}

std::string Game::get_name_lost(int side_to_move) const
{
  return "lost,side=" + std::to_string(side_to_move);
}

std::string Game::get_name_winning(int side_to_move, int ply_max) const
{
  std::ostringstream dfa_name_builder;
  dfa_name_builder << "ply_max=" << std::setfill('0') << std::setw(3) << ply_max << ",side=" << side_to_move << ",winning";
  return dfa_name_builder.str();
}

std::string Game::get_name_won(int side_to_move) const
{
  return "won,side=" + std::to_string(side_to_move);
}

shared_dfa_ptr Game::get_positions_losing(int side_to_move, int ply_max) const
{
  assert(ply_max >= 0);

  shared_dfa_ptr lost = get_positions_lost(side_to_move);
  if(ply_max <= 0)
    {
      return lost;
    }

  if(lost->is_constant(0))
    {
      // all losses will be in an odd number of ply
      if(ply_max % 2 == 0)
	{
	  --ply_max;
	}
    }

  return load_or_build(get_name_losing(side_to_move, ply_max),
		       [&]()
		       {
			 shared_dfa_ptr opponent_winning_sooner = get_positions_winning(1 - side_to_move, ply_max - 1);
			 shared_dfa_ptr opponent_not_winning_sooner = DFAUtil::get_inverse(opponent_winning_sooner);
			 shared_dfa_ptr not_losing_soon = get_moves_backward(side_to_move, opponent_not_winning_sooner);
			 shared_dfa_ptr losing_soon = DFAUtil::get_difference(get_has_moves(side_to_move), not_losing_soon);
			 return DFAUtil::get_union(losing_soon, lost);
		       });
}

shared_dfa_ptr Game::get_positions_winning(int side_to_move, int ply_max) const
{
  assert(ply_max >= 0);

  shared_dfa_ptr won = get_positions_won(side_to_move);
  if(ply_max <= 0)
    {
      return won;
    }

  if(won->is_constant(0))
    {
      // all wins will be in an odd number of ply
      if(ply_max % 2 == 0)
	{
	  --ply_max;
	}
    }

  return this->load_or_build(get_name_winning(side_to_move, ply_max),
			     [&]()
			     {
			       shared_dfa_ptr losing_sooner = this->get_positions_losing(1 - side_to_move, ply_max - 1);
			       shared_dfa_ptr winning_soon = this->get_moves_backward(side_to_move, losing_sooner);

			       return DFAUtil::get_union(won, winning_soon);
			     });
}

const dfa_shape_t& Game::get_shape() const
{
  return shape;
}

shared_dfa_ptr Game::load(std::string dfa_name_in) const
{
  Profile profile("load " + dfa_name_in);

  std::string dfa_name = name + "/" + dfa_name_in;

  return shared_dfa_ptr(new DFA(shape, dfa_name));
}

shared_dfa_ptr Game::load_by_hash(std::string hash_in) const
{
  return DFAUtil::load_by_hash(get_shape(), hash_in);
}

shared_dfa_ptr Game::load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr ()> build_func) const
{
  Profile profile("load_or_build " + dfa_name_in);

  std::string dfa_name = name + "/" + dfa_name_in;

  profile.tic("load");
  try
    {
      shared_dfa_ptr output = load(dfa_name_in);
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
