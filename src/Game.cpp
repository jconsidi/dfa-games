// Game.cpp

#include "Game.h"

#include <iostream>
#include <map>

#include "IntersectionManager.h"

template <int ndim, int... shape_pack>
Game<ndim, shape_pack...>::Game()
{
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_initial_positions() const
{
  if(!(this->singleton_initial_positions))
    {
      this->singleton_initial_positions = this->get_initial_positions_internal();
    }

  return this->singleton_initial_positions;
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_lost_positions(int side_to_move) const
{
  if(!(this->singleton_lost_positions[side_to_move]))
    {
      std::cout << "Game::get_lost_positions(" << side_to_move << ")" << std::endl;
      this->singleton_lost_positions[side_to_move] = this->get_lost_positions_internal(side_to_move);
      std::cout << "Game::get_lost_positions(" << side_to_move << ") => " << this->singleton_lost_positions[side_to_move]->states() << " states, " << this->singleton_lost_positions[side_to_move]->size() << " positions" << std::endl;
    }

  return this->singleton_lost_positions[side_to_move];
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_forward(int side_to_move, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  rule_vector rules = get_rules(side_to_move);
  return get_moves_internal(rules, positions_in);
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_internal(const typename Game<ndim, shape_pack...>::rule_vector& rules_in, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  std::cout << "get_moves_internal(...)" << std::endl;

  assert(rules_in.size() > 0);

  IntersectionManager<ndim, shape_pack...> manager;
  std::map<shared_dfa_ptr,std::vector<shared_dfa_ptr>> rule_outputs_by_post_condition;

  int num_rules = rules_in.size();
  for(int i = 0; i < num_rules; ++i)
    {
      std::cout << " rule " << i << "/" << num_rules << std::endl;

      const rule_type& rule = rules_in[i];

      const shared_dfa_ptr& pre_condition = std::get<0>(rule);
      const change_func& change_rule = std::get<1>(rule);
      const shared_dfa_ptr& post_condition = std::get<2>(rule);

      shared_dfa_ptr positions = 0;
      // apply rule pre-conditions
      positions = manager.intersect(positions_in, pre_condition);
      std::cout << "  pre-condition => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;
      // apply rule changes
      positions = shared_dfa_ptr(new change_dfa_type(*positions, change_rule));
      std::cout << "  changes => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;
      // defer rule post-conditions
      rule_outputs_by_post_condition.try_emplace(post_condition);
      rule_outputs_by_post_condition.at(post_condition).push_back(positions);
    }

  // combine rules with the same post-conditions, then apply post conditions

  std::vector<shared_dfa_ptr> rule_outputs;
  for(const auto& [post_condition, positions_vector] : rule_outputs_by_post_condition)
    {
      std::cout << " applying post condition to " << positions_vector.size() << " rule outputs" << std::endl;
      shared_dfa_ptr positions(new union_dfa_type(positions_vector));
      std::cout << "  combined rule outputs => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;

      positions = manager.intersect(positions, post_condition);
      std::cout << "  post-condition => " << positions->states() << " states, " << positions->size() << " positions" << std::endl;

      rule_outputs.push_back(positions);
    }

  // final output

  std::cout << " combining " << rule_outputs.size() << " post-condition rule outputs" << std::endl;
  return shared_dfa_ptr(new UnionDFA(rule_outputs));
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_moves_reverse(int side_to_move, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  rule_vector rules_forward = get_rules(side_to_move);

  rule_vector rules_reverse;
  std::for_each(rules_forward.cbegin(), rules_forward.cend(), [&](const rule_type& rule_forward)
  {
    const change_func& change_forward = std::get<1>(rule_forward);

    rules_reverse.emplace_back(std::get<2>(rule_forward),
			       [&](int layer, int old_value, int new_value)
			       {
				 return change_forward(layer, new_value, old_value);
			       },
			       std::get<0>(rule_forward));
  });

  return get_moves_internal(rules_reverse, positions_in);
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_winning_positions(int side_to_move, int moves_max) const
{
  return this->get_winning_positions(side_to_move, moves_max, 0);
}

template <int ndim, int... shape_pack>
typename Game<ndim, shape_pack...>::shared_dfa_ptr Game<ndim, shape_pack...>::get_winning_positions(int side_to_move, int moves_max, typename Game<ndim, shape_pack...>::shared_dfa_ptr positions_in) const
{
  int side_not_to_move = 1 - side_to_move;

  shared_dfa_ptr accept_all(new accept_dfa_type());
  shared_dfa_ptr opponent_has_move = this->get_moves_reverse(side_not_to_move, accept_all);

  shared_dfa_ptr lost = this->get_lost_positions(side_not_to_move);
  shared_dfa_ptr losing = lost;
  shared_dfa_ptr winning = this->get_moves_reverse(side_to_move, losing);
  std::cout << "  move 0: " << winning->size() << " winning positions, " << winning->states() << " states" << std::endl;

  for(int move = 1; move < moves_max; ++move)
    {
      shared_dfa_ptr not_yet_winning(new inverse_dfa_type(*winning));
      shared_dfa_ptr not_yet_losing(this->get_moves_reverse(side_not_to_move, not_yet_winning));
      shared_dfa_ptr losing_more_if_has_move(new inverse_dfa_type(*not_yet_losing));
      shared_dfa_ptr losing_more(new intersection_dfa_type(*losing_more_if_has_move, *opponent_has_move));
      if(difference_dfa_type(*losing_more, *losing).size() == 0)
	{
	  break;
	}

      losing = shared_dfa_ptr(new union_dfa_type(*losing, *losing_more));
      winning = this->get_moves_reverse(side_to_move, losing);
      std::cout << "  move " << move << ": " << winning->size() << " winning positions, " << winning->states() << " states" << std::endl;

      if(positions_in && (difference_dfa_type(*winning, *positions_in).size() == 0))
	{
	  std::cout << "  target positions covered" << std::endl;
	  break;
	}
    }

  if(positions_in)
    {
      winning = shared_dfa_ptr(new intersection_dfa_type(*winning, *positions_in));
    }

  return winning;
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class Game<CHESS_DFA_PARAMS>;
template class Game<TICTACTOE2_DFA_PARAMS>;
template class Game<TICTACTOE3_DFA_PARAMS>;
template class Game<TICTACTOE4_DFA_PARAMS>;
