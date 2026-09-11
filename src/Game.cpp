// Game.cpp

#include "Game.h"

#include <sys/stat.h>

#include <algorithm>
#include <format>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "DFAUtil.h"
#include "DNFBuilder.h"
#include "Profile.h"
#include "ScratchConfig.h"

Game::Game(std::string name_in, const dfa_shape_t& shape_in)
  : GameBase(name_in, shape_in, 2)
{
  // Rule derived helper caches (get_has_moves, ChessGame/OthelloGame's move
  // generation helpers) save non-durable under this same "<game>/<name>"
  // shape, so the local root needs this game's directory too -- GameBase's
  // own constructor already created the archive one for durable results
  // (lost, won, ...).
  std::string local_directory = ScratchConfig::get_local_dir() + "/" + name_in;
  mkdir(local_directory.c_str(), 0700);
}

shared_dfa_ptr Game::build_positions_losing(int side_to_move, int ply_max) const
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

  // recursive case

  shared_dfa_ptr opponent_winning_sooner = get_positions_winning(1 - side_to_move, ply_max - 1);
  shared_dfa_ptr opponent_not_winning_sooner = DFAUtil::get_inverse(opponent_winning_sooner);
  shared_dfa_ptr not_losing_soon = get_moves_backward(side_to_move, opponent_not_winning_sooner);
  shared_dfa_ptr losing_soon = DFAUtil::get_difference(get_has_moves(side_to_move), not_losing_soon);
  return DFAUtil::get_union(losing_soon, lost);
}

shared_dfa_ptr Game::build_positions_lost(int /* side_to_move */) const
{
  // default implementation. build_positions_lost or
  // build_positions_won should be overridden.

  return DFAUtil::get_reject(get_shape());
}

shared_dfa_ptr Game::build_positions_reversed(shared_dfa_ptr /* positions_in */) const
{
  // default no reverse support
  return shared_dfa_ptr(0);
}

shared_dfa_ptr Game::build_positions_winning(int side_to_move, int ply_max) const
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

  // recursive case

  shared_dfa_ptr losing_sooner = this->get_positions_losing(1 - side_to_move, ply_max - 1);
  shared_dfa_ptr winning_soon = this->get_moves_backward(side_to_move, losing_sooner);

  return DFAUtil::get_union(won, winning_soon);
}

shared_dfa_ptr Game::build_positions_won(int /* side_to_move */) const
{
  // default implementation. build_positions_lost or
  // build_positions_won should be overridden.

  return DFAUtil::get_reject(get_shape());
}

bool Game::can_reverse() const
{
  if(!reverse_implemented.has_value())
    {
      shared_dfa_ptr reject_reversed = build_positions_reversed(DFAUtil::get_reject(get_shape()));
      if(reject_reversed)
        {
          assert(reject_reversed->is_constant(0));
        }

      reverse_implemented = bool(reject_reversed);
    }

  assert(reverse_implemented.has_value());
  return *reverse_implemented;
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
			shared_dfa_ptr all_positions = DFAUtil::get_accept(get_shape());
			return this->get_moves_backward(side_to_move, all_positions);
		      },
		      false); // internal helper predicate, not solver output
    }

  return this->singleton_has_moves[side_to_move];
}

std::string Game::get_name_losing(int side_to_move, int ply_max) const
{
  return std::format("backward,ply_max={:03d},side={:d},losing",
		     ply_max, side_to_move);
}

std::string Game::get_name_lost(int side_to_move) const
{
  return "lost,side_to_move=" + std::to_string(side_to_move);
}

std::string Game::get_name_unknown(int side_to_move, int ply_max) const
{
  return std::format("backward,ply_max={:03d},side={:d},unknown",
		     ply_max, side_to_move);
}

std::string Game::get_name_winning(int side_to_move, int ply_max) const
{
  return std::format("backward,ply_max={:03d},side={:d},winning",
		     ply_max, side_to_move);
}

std::string Game::get_name_won(int side_to_move) const
{
  return "won,side_to_move=" + std::to_string(side_to_move);
}

shared_dfa_ptr Game::get_positions_forward(int ply) const
{
  assert(ply >= 0);

  std::ostringstream name_builder;
  name_builder << "forward,ply=" << std::setfill('0') << std::setw(3) << ply;
  return load_or_build(name_builder.str(),
		       [&]()
                       {
			 if(ply == 0)
			   {
			     return get_positions_initial();
			   }

			 shared_dfa_ptr previous = get_positions_forward(ply - 1);
			 return get_moves_forward((ply - 1) % 2, previous);
		       });
}

shared_dfa_ptr Game::get_positions_initial() const
{
  return DFAUtil::from_string(get_shape(), get_position_initial());
}

shared_dfa_ptr Game::get_positions_losing(int side_to_move, int ply_max) const
{
  assert(ply_max >= 0);

  return load_or_build(get_name_losing(side_to_move, ply_max), [&]()
  {
    if(this->can_reverse())
      {
        shared_dfa_ptr output_reversed = this->load_by_name(get_name_losing(1 - side_to_move, ply_max));
        if(output_reversed)
          {
            return build_positions_reversed(output_reversed);
          }
      }

    return build_positions_losing(side_to_move, ply_max);
  });
}

shared_dfa_ptr Game::get_positions_lost(int side_to_move) const
{
  return this->load_or_build(get_name_lost(side_to_move),
			     [&]()
			     {
			       return build_positions_lost(side_to_move);
			     });
}

shared_dfa_ptr Game::get_positions_reachable(int side_to_move, int ply) const
{
  assert((0 <= side_to_move) && (side_to_move < 2));
  assert(0 <= ply);

  if(ply == 0)
    {
      return DFAUtil::get_accept(get_shape());
    }

  std::ostringstream dfa_name_builder;
  dfa_name_builder << "reachable,side_to_move=" << std::to_string(side_to_move) << ",ply=" << std::to_string(ply);

  return load_or_build(dfa_name_builder.str(), [&]()
  {
    shared_dfa_ptr previous = get_positions_reachable(1 - side_to_move,
						      ply - 1);
    return get_moves_forward(1 - side_to_move,
			     previous);
  });
}

shared_dfa_ptr Game::get_positions_unknown(int side_to_move, int ply_max) const
{
  assert(ply_max >= 0);

  return this->load_or_build(get_name_unknown(side_to_move, ply_max), [&]()
  {
    shared_dfa_ptr losing = this->get_positions_losing(side_to_move, ply_max);
    shared_dfa_ptr winning = this->get_positions_winning(side_to_move, ply_max);

    shared_dfa_ptr decided = DFAUtil::get_union(losing, winning);
    shared_dfa_ptr output = DFAUtil::get_inverse(decided);

#ifdef PARANOIA
    shared_dfa_ptr check_decided = DFAUtil::get_intersection(decided, output);
    assert(check_decided->is_constant(0));

    shared_dfa_ptr check_winning_subset = DFAUtil::get_difference(winning, decided);
    assert(check_winning_subset->is_constant(0));

    shared_dfa_ptr check_winning = DFAUtil::get_intersection(winning, output);
    assert(check_winning->is_constant(0));

    shared_dfa_ptr check_losing = DFAUtil::get_intersection(losing, output);
    assert(check_losing->is_constant(0));
#endif

    return output;
  });
}

shared_dfa_ptr Game::get_positions_winning(int side_to_move, int ply_max) const
{
  assert(ply_max >= 0);

  return this->load_or_build(get_name_winning(side_to_move, ply_max), [&]()
  {
    if(this->can_reverse())
      {
        shared_dfa_ptr output_reversed = this->load_by_name(get_name_winning(1 - side_to_move, ply_max));
        if(output_reversed)
          {
            return build_positions_reversed(output_reversed);
          }
      }

    // optimize case where number of unknown states is small
    if(ply_max > 0)
      {
        // soon = ply_max - 1
        shared_dfa_ptr losing_soon = get_positions_losing(1 - side_to_move, ply_max - 1);
        shared_dfa_ptr unknown_soon = get_positions_unknown(side_to_move, ply_max - 1);

        if(unknown_soon->states() <= losing_soon->states() / 10)
          {
            shared_dfa_ptr unknown_to_losing = DFAUtil::get_intersection(this->get_moves_forward(side_to_move, unknown_soon), losing_soon);

            // winning in exactly ply_max ply.
            shared_dfa_ptr winning_new = DFAUtil::get_intersection(this->get_moves_backward(side_to_move, unknown_to_losing), unknown_soon);

            shared_dfa_ptr winning_soon = get_positions_winning(side_to_move, ply_max - 1);

            return DFAUtil::get_union(winning_new, winning_soon);
          }
      }

    return build_positions_winning(side_to_move, ply_max);
  });
}

shared_dfa_ptr Game::get_positions_won(int side_to_move) const
{
  return this->load_or_build(get_name_won(side_to_move),
			     [&]()
			     {
			       return build_positions_won(side_to_move);
			     });
}

shared_dfa_ptr Game::load_by_hash(std::string hash_in, bool durable) const
{
  return DFAUtil::load_by_hash(get_shape(), hash_in, durable);
}

shared_dfa_ptr Game::load_by_name(std::string dfa_name_in, bool durable) const
{
  // load by name, but return NULL when there's an issue
  try
    {
      std::string dfa_name = get_name() + "/" + dfa_name_in;
      return DFAUtil::load_by_name(get_shape(), dfa_name, durable);
    }
  catch(const std::runtime_error& e)
    {
      return shared_dfa_ptr(0);
    }
}

shared_dfa_ptr Game::load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr ()> build_func, bool durable) const
{
  Profile profile("load_or_build " + dfa_name_in);

  std::string dfa_name = get_name() + "/" + dfa_name_in;
  return DFAUtil::load_or_build(get_shape(), dfa_name, build_func, durable);
}
