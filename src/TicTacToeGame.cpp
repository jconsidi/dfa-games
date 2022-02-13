// TicTacToeGame.cpp

#include "TicTacToeGame.h"

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::shared_dfa_ptr TicTacToeGame<n, shape_pack...>::get_initial_positions_internal() const
{
  shared_dfa_ptr output(new accept_dfa_type());

  for(int i = 0; i < n * n; ++i)
    {
      fixed_dfa_type blank_i(i, 0);
      output = shared_dfa_ptr(new intersection_dfa_type(*output, blank_i));
    }

  return output;
}

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::shared_dfa_ptr TicTacToeGame<n, shape_pack...>::get_lost_condition(int side_to_move, int x_start, int y_start, int x_delta, int y_delta)
{
  shared_dfa_ptr condition(new accept_dfa_type());

  for(int i = 0; i < n; ++i)
    {
      int x = x_start + x_delta * i;
      int y = y_start + y_delta * i;
      int index = y * n + x;

      fixed_dfa_type opponent_piece(index, 2 - side_to_move);
      condition = shared_dfa_ptr(new intersection_dfa_type(*condition, opponent_piece));
    }

  return condition;
}

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::shared_dfa_ptr TicTacToeGame<n, shape_pack...>::get_lost_positions_internal(int side_to_move) const
{
  shared_dfa_ptr lost_positions(new reject_dfa_type());

  // vertical lost conditions
  for(int x = 0; x < n; ++x)
    {
      shared_dfa_ptr lost_vertical = get_lost_condition(side_to_move, x, 0, 0, 1);
      lost_positions = shared_dfa_ptr(new union_dfa_type(*lost_positions, *lost_vertical));
    }

  // horizontal lost conditions
  for(int y = 0; y < n; ++y)
    {
      shared_dfa_ptr lost_horizontal = get_lost_condition(side_to_move, 0, y, 1, 0);
      lost_positions = shared_dfa_ptr(new union_dfa_type(*lost_positions, *lost_horizontal));
    }

  // diagonal lost conditions

  shared_dfa_ptr diagonal_plus = get_lost_condition(side_to_move, 0, 0, 1, 1);
  lost_positions = shared_dfa_ptr(new union_dfa_type(*lost_positions, *diagonal_plus));

  shared_dfa_ptr diagonal_negative = get_lost_condition(side_to_move, 0, n - 1, 1, -1);
  lost_positions = shared_dfa_ptr(new union_dfa_type(*lost_positions, *diagonal_negative));

  // done

  return lost_positions;
}

template<int n, int... shape_pack>
const typename TicTacToeGame<n, shape_pack...>::rule_vector& TicTacToeGame<n, shape_pack...>::get_rules(int side_to_move) const
{
  static rule_vector singletons[2] = {};

  if(!singletons[side_to_move].size())
    {
      shared_dfa_ptr lost_positions = this->get_lost_positions(side_to_move);
      inverse_dfa_type not_lost_positions(*lost_positions);

      int side_to_move_piece = 1 + side_to_move;

      rule_vector output;
      for(int move_index = 0; move_index < n * n; ++move_index)
	{
	  shared_dfa_ptr pre_condition(new intersection_dfa_type(not_lost_positions, fixed_dfa_type(move_index, 0)));

	  change_func change_rule = [=](int layer, int old_value, int new_value)
	  {
	    if(layer == move_index)
	      {
		if(old_value == 0)
		  {
		    // square was blank, changing to side to move
		    return new_value == side_to_move_piece;
		  }
		else
		  {
		    // square was not blank as required
		    return false;
		  }
	      }
	    else
	      {
		// square not changing
		return old_value == new_value;
	      }
	  };

	  shared_dfa_ptr post_condition(new intersection_dfa_type(not_lost_positions, fixed_dfa_type(move_index, 1 + side_to_move)));

	  singletons[side_to_move].emplace_back(pre_condition,
						change_rule,
						post_condition);
	}

      assert(singletons[side_to_move].size() == n * n);
    }

  return singletons[side_to_move];
}

// template instantiations

template class TicTacToeGame<2, TICTACTOE2_SHAPE_PACK>;
template class TicTacToeGame<3, TICTACTOE3_SHAPE_PACK>;
template class TicTacToeGame<4, TICTACTOE4_SHAPE_PACK>;
