// Game.cpp

#include "Game.h"

#include <iostream>
#include <map>
#include <string>

#include "IntersectionManager.h"
#include "Profile.h"

template <int ndim, int... shape_pack>
Game<ndim, shape_pack...>::Game(std::string name_in)
  : name(name_in)
{
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_has_moves(int side_to_move) const
{
  if(!(this->singleton_has_moves[side_to_move]))
    {
      this->singleton_has_moves[side_to_move] = \
	load_or_build("has_moves-side=" + std::to_string(side_to_move),
		      [=]()
		      {
			shared_dfa_ptr all_positions(new accept_dfa_type());
			return this->get_moves_reverse(side_to_move, all_positions);
		      });
    }

  return this->singleton_has_moves[side_to_move];
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_initial_positions() const
{
  if(!(this->singleton_initial_positions))
    {
      this->singleton_initial_positions = \
	load_or_build("initial_positions",
		      [=]()
		      {
			return this->get_initial_positions_internal();
		      });
    }

  return this->singleton_initial_positions;
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_lost_positions(int side_to_move) const
{
  if(!(this->singleton_lost_positions[side_to_move]))
    {
      this->singleton_lost_positions[side_to_move] = \
	load_or_build("lost_positions-side=" + std::to_string(side_to_move),
		      [=]()
		      {
			return this->get_lost_positions_internal(side_to_move);
		      });
    }

  return this->singleton_lost_positions[side_to_move];
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_forward(int side_to_move, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_forward");

  profile.tic("get_rules");
  rule_vector rules = get_rules(side_to_move);

  profile.tic("get_moves_internal");
  return get_moves_internal(rules, positions_in);
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_internal(const typename Game<ndim, shape_pack...>::rule_vector& rules_in, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_internal");

  std::cout << "get_moves_internal(...)" << std::endl;

  assert(rules_in.size() > 0);

  IntersectionManager<ndim, shape_pack...> manager;
  std::map<shared_dfa_ptr,std::vector<shared_dfa_ptr>> rule_outputs_by_post_condition;

  int num_rules = rules_in.size();
  for(int i = 0; i < num_rules; ++i)
    {
      profile.tic("rule init");

      const rule_type& rule = rules_in[i];

      std::cout << " rule " << i << "/" << num_rules << ": " << std::get<3>(rule) << std::endl;

      const shared_dfa_ptr& pre_condition = std::get<0>(rule);
      const change_func& change_rule = std::get<1>(rule);
      const shared_dfa_ptr& post_condition = std::get<2>(rule);

      shared_dfa_ptr positions = 0;

      // apply rule pre-conditions
      profile.tic("rule pre condition");
      positions = manager.intersect(positions_in, pre_condition);
      std::cout << "  pre-condition => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;

      // apply rule changes
      profile.tic("rule change");
      positions = shared_dfa_ptr(new change_dfa_type(*positions, change_rule));
      std::cout << "  changes => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;

      // defer rule post-conditions
      profile.tic("rule defer");
      rule_outputs_by_post_condition.try_emplace(post_condition);
      rule_outputs_by_post_condition.at(post_condition).push_back(positions);

      // partially merge the rule outputs to reduce memory usage
      profile.tic("rule merge partial");
      auto& current_outputs = rule_outputs_by_post_condition.at(post_condition);
      while(current_outputs.size() > 1)
	{
	  int s = current_outputs.size();
	  if(current_outputs[s-2]->states() > current_outputs[s-1]->states())
	    {
	      // remaining sizes are decreasing. so handwavy logarithmic number of them.
	      break;
	    }

	  // merge the last two entries
	  shared_dfa_ptr d1 = current_outputs.back();
	  current_outputs.pop_back();
	  shared_dfa_ptr d2 = current_outputs.back();
	  current_outputs.pop_back();
	  current_outputs.emplace_back(new union_dfa_type(*d1, *d2));
	}
      std::cout << "  " << current_outputs.size() << " partially combined rule outputs with this post-condition, biggest => " << current_outputs[0]->states() << " states" << std::endl;
    }

  // combine rules with the same post-conditions, then apply post conditions

  std::vector<shared_dfa_ptr> rule_outputs;
  for(const auto& [post_condition, positions_vector] : rule_outputs_by_post_condition)
    {
      std::cout << " applying post condition to " << positions_vector.size() << " rule outputs" << std::endl;
      profile.tic("post merge");
      shared_dfa_ptr positions(new union_dfa_type(positions_vector));
      std::cout << "  combined rule outputs => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;

      profile.tic("post condition");
      positions = manager.intersect(positions, post_condition);
      std::cout << "  post-condition => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;

      rule_outputs.push_back(positions);
    }

  // final output

  profile.tic("final merge");
  std::cout << " combining " << rule_outputs.size() << " post-condition rule outputs" << std::endl;
  return shared_dfa_ptr(new UnionDFA(rule_outputs));
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_reverse(int side_to_move, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_reverse");

  profile.tic("get_rules");
  rule_vector rules_forward = get_rules(side_to_move);

  profile.tic("rules_reverse");
  rule_vector rules_reverse;
  std::for_each(rules_forward.cbegin(), rules_forward.cend(), [&](const rule_type& rule_forward)
  {
    const change_func& change_forward = std::get<1>(rule_forward);

    rules_reverse.emplace_back(std::get<2>(rule_forward),
			       [&](int layer, int old_value, int new_value)
			       {
				 return change_forward(layer, new_value, old_value);
			       },
			       std::get<0>(rule_forward),
			       std::get<3>(rule_forward));
  });

  profile.tic("get_moves_internal");
  return get_moves_internal(rules_reverse, positions_in);
}

template <int ndim, int... shape_pack>
const typename Game<ndim, shape_pack...>::rule_vector& Game<ndim, shape_pack...>::get_rules(int side_to_move) const
{
  assert((0 <= side_to_move) && (side_to_move < 2));

  if(singleton_rules[side_to_move].size() == 0)
    {
      singleton_rules[side_to_move] = this->get_rules_internal(side_to_move);
    }

  return singleton_rules[side_to_move];
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_winning_positions(int side_to_move, int moves_max) const
{
  return this->get_winning_positions(side_to_move, moves_max, 0);
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_winning_positions(int side_to_move, int moves_max, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  Profile profile("get_winning_positions");

  int side_not_to_move = 1 - side_to_move;

  profile.tic("opponent_has_move");
  shared_dfa_ptr opponent_has_move = this->get_has_moves(side_not_to_move);

  profile.tic("lost");
  shared_dfa_ptr lost = this->get_lost_positions(side_not_to_move);
  shared_dfa_ptr losing = lost;

  profile.tic("winning 0");
  shared_dfa_ptr winning = this->get_moves_reverse(side_to_move, losing);
  std::cout << "  move 0: " << winning->size() << " winning positions, " << winning->states() << " states" << std::endl;

  for(int move = 1; move < moves_max; ++move)
    {
      auto tic = [&](std::string label_in){profile.tic(label_in + " " + std::to_string(move));};

      tic("not_yet_winning");
      shared_dfa_ptr not_yet_winning(new inverse_dfa_type(*winning));

      tic("not_yet_losing");
      shared_dfa_ptr not_yet_losing(this->get_moves_reverse(side_not_to_move, not_yet_winning));

      tic("losing_more_if_has_move");
      shared_dfa_ptr losing_more_if_has_move(new inverse_dfa_type(*not_yet_losing));

      tic("losing_more");
      shared_dfa_ptr losing_more(new intersection_dfa_type(*losing_more_if_has_move, *opponent_has_move));

      tic("finished check");
      if(difference_dfa_type(*losing_more, *losing).size() == 0)
	{
	  break;
	}

      tic("losing");
      losing = shared_dfa_ptr(new union_dfa_type(*losing, *losing_more));

      tic("winning");
      winning = this->get_moves_reverse(side_to_move, losing);
      std::cout << "  move " << move << ": " << winning->size() << " winning positions, " << winning->states() << " states" << std::endl;

      tic("stats");
      if(positions_in && (difference_dfa_type(*winning, *positions_in).size() == 0))
	{
	  std::cout << "  target positions covered" << std::endl;
	  break;
	}
    }

  if(positions_in)
    {
      profile.tic("filter winning");
      winning = shared_dfa_ptr(new intersection_dfa_type(*winning, *positions_in));
    }

  return winning;
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
      std::cout << "loaded " << dfa_name << " => " << output->states() << " states" << std::endl;

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
