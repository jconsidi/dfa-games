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

template <int ndim, int... shape_pack>
std::string quick_stats(const DFA<ndim, shape_pack...>& dfa_in)
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

template <int ndim, int... shape_pack>
void sort_rules(typename Game<ndim, shape_pack...>::rule_vector& rules)
{
  Profile profile("sort_rules");

  std::stable_sort(rules.begin(), rules.end(), [](const typename Game<ndim, shape_pack...>::rule_type& a,
						  const typename Game<ndim, shape_pack...>::rule_type& b)
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

    // do not compare change rule
    return false;
  });
}

template <int ndim, int... shape_pack>
Game<ndim, shape_pack...>::Game(std::string name_in)
  : name(name_in)
{
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_has_moves(int side_to_move) const
{
  Profile profile("get_has_moves");

  if(!(this->singleton_has_moves[side_to_move]))
    {
      this->singleton_has_moves[side_to_move] = \
	load_or_build("has_moves-side=" + std::to_string(side_to_move),
		      [=]()
		      {
			shared_dfa_ptr all_positions = DFAUtil<ndim, shape_pack...>::get_accept();
			return this->get_moves_backward(side_to_move, all_positions);
		      });
    }

  return this->singleton_has_moves[side_to_move];
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_forward(int side_to_move, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_forward");

  const rule_vector& rules = get_rules_forward(side_to_move);

  return get_moves_internal(rules, positions_in);
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_internal(const typename Game<ndim, shape_pack...>::rule_vector& rules_in, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_internal");

  std::cout << "get_moves_internal(...)" << std::endl;

  assert(rules_in.size() > 0);

  // sort rules to improve intersection sharing...

  profile.tic("sort rules");
  rule_vector rules = rules_in;
  sort_rules<ndim, shape_pack...>(rules);

  profile.tic("data structure setup");

  IntersectionManager<ndim, shape_pack...> manager;
  DNFBuilder<ndim, shape_pack...> output_builder;

  int num_rules = rules.size();
  for(int i = 0; i < num_rules; ++i)
    {
      profile.tic("rule init");

      const rule_type& rule = rules[i];

      std::cout << " rule " << i << "/" << num_rules << ": " << std::get<3>(rule) << std::endl;

      const std::vector<shared_dfa_ptr> pre_conditions = std::get<0>(rule);
      const change_vector& changes = std::get<1>(rule);
      const std::vector<shared_dfa_ptr> post_conditions = std::get<2>(rule);

      shared_dfa_ptr positions = positions_in;

      // apply rule pre-conditions
      profile.tic("rule pre conditions");
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

      // apply rule changes
      profile.tic("rule change");
      positions = shared_dfa_ptr(new change_dfa_type(*positions, changes));
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

  // final output

  profile.tic("final output");

  return output_builder.to_dfa();
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_backward(int side_to_move, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_backward");

  const rule_vector& rules_backward = get_rules_backward(side_to_move);

  return get_moves_internal(rules_backward, positions_in);
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_positions_losing(int side_to_move, int ply_max) const
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

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_positions_winning(int side_to_move, int ply_max) const
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
      shared_dfa_ptr not_yet_winning = DFAUtil<ndim, shape_pack...>::get_inverse(winning);

      tic("not_yet_losing");
      shared_dfa_ptr not_yet_losing(this->get_moves_backward(side_not_to_move, not_yet_winning));

      tic("losing_more_if_has_move");
      shared_dfa_ptr losing_more_if_has_move = DFAUtil<ndim, shape_pack...>::get_inverse(not_yet_losing);

      tic("losing_more");
      shared_dfa_ptr losing_more = DFAUtil<ndim, shape_pack...>::get_intersection(losing_more_if_has_move, opponent_has_move);

      tic("finished check");
      if(DFAUtil<ndim, shape_pack...>::get_difference(losing_more, losing)->is_constant(false))
	{
	  break;
	}

      tic("losing");
      losing = DFAUtil<ndim, shape_pack...>::get_union(losing, losing_more);

      tic("winning");
      winning = this->get_moves_backward(side_to_move, losing);
      std::cout << "  move " << move << ": " << winning->size() << " winning positions, " << winning->states() << " states" << std::endl;
    }

  return winning;
}

template <int ndim, int... shape_pack>
const typename Game<ndim, shape_pack...>::rule_vector& Game<ndim, shape_pack...>::get_rules_forward(int side_to_move) const
{
  Profile profile("get_rules_forward");

  assert((0 <= side_to_move) && (side_to_move < 2));

  if(singleton_rules_forward[side_to_move].size() == 0)
    {
      singleton_rules_forward[side_to_move] = this->get_rules_internal(side_to_move);
    }

  return singleton_rules_forward[side_to_move];
}

template <int ndim, int... shape_pack>
const typename Game<ndim, shape_pack...>::rule_vector& Game<ndim, shape_pack...>::get_rules_backward(int side_to_move) const
{
  Profile profile("get_rules_backward");

  assert((0 <= side_to_move) && (side_to_move < 2));

  if(singleton_rules_backward[side_to_move].size() == 0)
    {
      const rule_vector& rules_forward = get_rules_forward(side_to_move);

      rule_vector& rules_backward = singleton_rules_backward[side_to_move];

      for(const rule_type& rule_forward : rules_forward)
	{
	  const change_vector& changes_forward = std::get<1>(rule_forward);

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

	  rules_backward.emplace_back(std::get<2>(rule_forward),
				     changes_backward,
				     std::get<0>(rule_forward),
				     std::get<3>(rule_forward));
	}
    }

  return singleton_rules_backward[side_to_move];
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::load_or_build(std::string dfa_name_in, std::function<typename Game<ndim, shape_pack...>::shared_dfa_ptr ()> build_func) const
{
  Profile profile("load_or_build " + dfa_name_in);

  std::string dfa_name = name + "/" + dfa_name_in;

  profile.tic("load");
  try
    {
      shared_dfa_ptr output = shared_dfa_ptr(new dfa_type(dfa_name));
      std::cout << "loaded " << dfa_name << " => " << quick_stats(*output) << std::endl;

      return output;
    }
  catch(std::runtime_error e)
    {
    }

  profile.tic("build");
  std::cout << "building " << dfa_name << std::endl;
  shared_dfa_ptr output = build_func();

  profile.tic("stats");
  std::cout << "built " << dfa_name << " => " << output->size() << " positions, " << output->states() << " states" << std::endl;

  profile.tic("save");
  output->save(dfa_name);
  return output;
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class Game<CHESS_DFA_PARAMS>;
template class Game<TICTACTOE2_DFA_PARAMS>;
template class Game<TICTACTOE3_DFA_PARAMS>;
template class Game<TICTACTOE4_DFA_PARAMS>;
