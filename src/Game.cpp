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

std::string quick_stats(const DFA& dfa_in)
{
  std::ostringstream stats_builder;

  size_t states = dfa_in.states();
  stats_builder << states << " states";

  if(states <= 1000000)
    {
      stats_builder << ", " << dfa_in.size() << " positions";
    }

  return stats_builder.str();
}

void sort_choices(typename Game::choice_vector& choices)
{
  Profile profile("sort_choices");

  std::stable_sort(choices.begin(), choices.end(), [](const typename Game::choice_type& a,
						  const typename Game::choice_type& b)
  {
    // compare post conditions
    //
    // post condition order drives DNFBuilder merge strategy

    // TODO : change the condition comparisons to be consistent
    // instead of current pointer comparison

    auto post_a = std::get<2>(a);
    auto post_b = std::get<2>(b);
    for(int i = 0; (i < post_a.size()) && (i < post_b.size()); ++i)
      {
	// sort more states first, so reversing comparisons
	if(post_a[i]->states() > post_b[i]->states())
	  {
	    return true;
	  }
	else if(post_a[i]->states() < post_b[i]->states())
	  {
	    return false;
	  }

	if(post_a[i] < post_b[i])
	  {
	    return true;
	  }
	else if(post_a[i] > post_b[i])
	  {
	    return false;
	  }
      }

    if(post_a.size() < post_b.size())
      {
	return true;
      }
    else if(post_b.size() < post_b.size())
      {
	return false;
      }

    // compare pre conditions

    auto pre_a = std::get<0>(a);
    auto pre_b = std::get<0>(b);
    for(int i = 0; (i < pre_a.size()) && (i < pre_b.size()); ++i)
      {
	// sort more states first, so reversing comparisons
	if(pre_a[i]->states() > pre_b[i]->states())
	  {
	    return true;
	  }
	else if(pre_a[i]->states() < pre_b[i]->states())
	  {
	    return false;
	  }

	if(pre_a[i] < pre_b[i])
	  {
	    return true;
	  }
	else if(pre_a[i] > pre_b[i])
	  {
	    return false;
	  }
      }

    if(pre_a.size() < pre_b.size())
      {
	return true;
      }
    else if(pre_b.size() < pre_b.size())
      {
	return false;
      }

    // do not compare change choice
    return false;
  });
}

Game::Game(std::string name_in, const dfa_shape_t& shape_in)
  : name(name_in),
    shape(shape_in)
{
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

  const phase_vector& phases_backward = get_phases_backward(side_to_move);

  return get_moves_internal(phases_backward, positions_in);
}

shared_dfa_ptr Game::get_moves_forward(int side_to_move, shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_forward");

  const phase_vector& phases_forward = get_phases_forward(side_to_move);

  return get_moves_internal(phases_forward, positions_in);
}

shared_dfa_ptr Game::get_moves_internal(const typename Game::phase_vector& phases_in, shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_internal");

  std::cout << "get_moves_internal(...)" << std::endl;

  assert(phases_in.size() > 0);
  for(const choice_vector& choices_in : phases_in)
    {
      assert(choices_in.size() > 0);
    }

  // apply choices for each phase

  shared_dfa_ptr phase_positions = positions_in;
  for(const choice_vector& choices : phases_in)
    {
      IntersectionManager manager(shape);
      DNFBuilder output_builder(shape);

      int num_choices = choices.size();
      for(int i = 0; i < num_choices; ++i)
	{
	  profile.tic("choice init");

	  const choice_type& choice = choices[i];

	  std::cout << " choice " << i << "/" << num_choices << ": " << std::get<3>(choice) << std::endl;

	  const std::vector<shared_dfa_ptr> pre_conditions = std::get<0>(choice);
	  const change_vector& changes = std::get<1>(choice);
	  const std::vector<shared_dfa_ptr> post_conditions = std::get<2>(choice);

	  shared_dfa_ptr positions = phase_positions;

	  // apply choice pre-conditions
	  profile.tic("choice pre conditions");
	  for(shared_dfa_ptr pre_condition : pre_conditions)
	    {
	      positions = manager.intersect(positions, pre_condition);
#ifndef PARANOIA
	      if(positions->is_constant(false))
		{
		  // no matching positions
		  break;
		}
#endif
	    }
	  std::cout << "  pre-conditions => " << quick_stats(*positions) << std::endl;

#ifndef PARANOIA
	  if(positions->is_constant(false))
	    {
	      // no matching positions
	      continue;
	    }
#endif

	  // apply choice changes
	  profile.tic("choice change");
	  positions = shared_dfa_ptr(new ChangeDFA(*positions, changes));
	  std::cout << "  changes => " << quick_stats(*positions) << std::endl;

#ifndef PARANOIA
	  if(positions->is_constant(false))
	    {
	      // no matching positions
	      continue;
	    }
#endif

	  // add changed positions and post conditions to output builder

	  profile.tic("add output clause");

	  std::vector<shared_dfa_ptr> output_clause(post_conditions);
	  output_clause.push_back(positions);
	  output_builder.add_clause(output_clause);
	}

      // finish phase

      profile.tic("finish phase");

      phase_positions = output_builder.to_dfa();
    }

  return phase_positions;
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

const typename Game::phase_vector& Game::get_phases_backward(int side_to_move) const
{
  Profile profile("get_phases_backward");

  assert((0 <= side_to_move) && (side_to_move < 2));

  if(singleton_phases_backward[side_to_move].size() == 0)
    {
      const phase_vector& phases_forward = get_phases_forward(side_to_move);
      for(int phase_index = phases_forward.size() - 1; phase_index >= 0; --phase_index)
	{
	  const choice_vector& choices_forward = phases_forward[phase_index];

	  singleton_phases_backward[side_to_move].emplace_back();
	  choice_vector& choices_backward = singleton_phases_backward[side_to_move].back();

	  for(const choice_type& choice_forward : choices_forward)
	    {
	      const change_vector& changes_forward = std::get<1>(choice_forward);

	      change_vector changes_backward;
	      for(int i = 0; i < changes_forward.size(); ++i)
		{
		  const change_optional& change_forward = changes_forward[i];

		  if(change_forward.has_value())
		    {
		      changes_backward.emplace_back(change_type(std::get<1>(*change_forward), std::get<0>(*change_forward)));
		    }
		  else
		    {
		      changes_backward.emplace_back();
		    }
		}

	      choices_backward.emplace_back(std::get<2>(choice_forward),
					  changes_backward,
					  std::get<0>(choice_forward),
					  std::get<3>(choice_forward));
	    }
	  sort_choices(choices_backward);
	}
    }

  return singleton_phases_backward[side_to_move];
}

const typename Game::phase_vector& Game::get_phases_forward(int side_to_move) const
{
  Profile profile("get_phases_forward");

  assert((0 <= side_to_move) && (side_to_move < 2));

  if(singleton_phases_forward[side_to_move].size() == 0)
    {
      singleton_phases_forward[side_to_move] = this->get_phases_internal(side_to_move);
      for(choice_vector& choices : singleton_phases_forward[side_to_move])
	{
	  sort_choices(choices);
	}
    }

  return singleton_phases_forward[side_to_move];
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
      std::cout << "loaded " << dfa_name << " => " << quick_stats(*output) << std::endl;

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
