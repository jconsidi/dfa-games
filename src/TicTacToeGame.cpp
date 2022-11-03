// TicTacToeGame.cpp

#include "TicTacToeGame.h"

#include <sstream>
#include <string>

#include "DFAUtil.h"

template<int n, int... shape_pack>
TicTacToeGame<n, shape_pack...>::TicTacToeGame()
  : Game<n*n, shape_pack...>("tictactoe" + std::to_string(n))
{
}

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::shared_dfa_ptr TicTacToeGame<n, shape_pack...>::get_positions_initial() const
{
  shared_dfa_ptr output = DFAUtil<n*n, shape_pack...>::get_accept();

  for(int i = 0; i < n * n; ++i)
    {
      shared_dfa_ptr blank_i = DFAUtil<n*n, shape_pack...>::get_fixed(i, 0);
      output = DFAUtil<n*n, shape_pack...>::get_intersection(output, blank_i);
    }

  return output;
}

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::shared_dfa_ptr TicTacToeGame<n, shape_pack...>::get_lost_condition(int side_to_move, int x_start, int y_start, int x_delta, int y_delta)
{
  shared_dfa_ptr condition = DFAUtil<n*n, shape_pack...>::get_accept();

  for(int i = 0; i < n; ++i)
    {
      int x = x_start + x_delta * i;
      int y = y_start + y_delta * i;
      int index = y * n + x;

      shared_dfa_ptr opponent_piece = DFAUtil<n*n, shape_pack...>::get_fixed(index, 2 - side_to_move);
      condition = DFAUtil<n*n, shape_pack...>::get_intersection(condition, opponent_piece);
    }

  return condition;
}

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::shared_dfa_ptr TicTacToeGame<n, shape_pack...>::get_positions_lost(int side_to_move) const
{
  shared_dfa_ptr lost_positions = DFAUtil<n*n, shape_pack...>::get_reject();

  // vertical lost conditions
  for(int x = 0; x < n; ++x)
    {
      shared_dfa_ptr lost_vertical = get_lost_condition(side_to_move, x, 0, 0, 1);
      lost_positions = DFAUtil<n*n, shape_pack...>::get_union(lost_positions, lost_vertical);
    }

  // horizontal lost conditions
  for(int y = 0; y < n; ++y)
    {
      shared_dfa_ptr lost_horizontal = get_lost_condition(side_to_move, 0, y, 1, 0);
      lost_positions = DFAUtil<n*n, shape_pack...>::get_union(lost_positions, lost_horizontal);
    }

  // diagonal lost conditions

  shared_dfa_ptr diagonal_plus = get_lost_condition(side_to_move, 0, 0, 1, 1);
  lost_positions = DFAUtil<n*n, shape_pack...>::get_union(lost_positions, diagonal_plus);

  shared_dfa_ptr diagonal_negative = get_lost_condition(side_to_move, 0, n - 1, 1, -1);
  lost_positions = DFAUtil<n*n, shape_pack...>::get_union(lost_positions, diagonal_negative);

  // done

  return lost_positions;
}

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::shared_dfa_ptr TicTacToeGame<n, shape_pack...>::get_positions_won(int side_to_move) const
{
  return DFAUtil<n*n, shape_pack...>::get_reject();
}

template<int n, int... shape_pack>
typename TicTacToeGame<n, shape_pack...>::phase_vector TicTacToeGame<n, shape_pack...>::get_phases_internal(int side_to_move) const
{
  shared_dfa_ptr lost_positions = this->get_positions_lost(side_to_move);
  shared_dfa_ptr not_lost_positions = DFAUtil<n*n, shape_pack...>::get_inverse(lost_positions);

  int side_to_move_piece = 1 + side_to_move;

  phase_vector output(1);
  for(int move_index = 0; move_index < n * n; ++move_index)
    {
      std::vector<shared_dfa_ptr> pre_conditions;
      pre_conditions.push_back(not_lost_positions);
      pre_conditions.push_back(DFAUtil<n*n, shape_pack...>::get_fixed(move_index, 0));

      change_vector changes;
      for(int layer = 0; layer < n * n; ++layer)
	{
	  if(layer == move_index)
	    {
	      changes.emplace_back(change_type(0, side_to_move_piece));
	    }
	  else
	    {
	      changes.emplace_back();
	    }
	}

      std::vector<shared_dfa_ptr> post_conditions;
      post_conditions.emplace_back(not_lost_positions);
      post_conditions.emplace_back(DFAUtil<n*n, shape_pack...>::get_fixed(move_index, 1 + side_to_move));

      output[0].emplace_back(pre_conditions,
			     changes,
			     post_conditions,
			     "move_index=" + std::to_string(move_index));
    }

  assert(output[0].size() == n * n);

  return output;
}

template<int n, int... shape_pack>
std::string TicTacToeGame<n, shape_pack...>::position_to_string(const dfa_string_type& position_in) const
{
  std::ostringstream builder;

  int index = 0;
  for(int row = 0; row < n; ++row)
    {
      for(int col = 0; col < n; ++col, ++index)
	{
	  int c = position_in[index];
	  if(c == 0)
	    {
	      builder << " ";
	    }
	  else
	    {
	      builder << (c - 1);
	    }

	  if(col + 1 < n)
	    {
	      builder << "|";
	    }
	}
      builder << std::endl;

      if(row + 1 < n)
	{
	  for(int i = 0; i < 2 * n - 1; ++i)
	    {
	      builder << ((i % 2 == 0) ? "-" : "+");
	    }
	  builder << std::endl;
	}
    }
  assert(index == n * n);

  return builder.str();
}

// template instantiations

template class TicTacToeGame<2, TICTACTOE2_SHAPE_PACK>;
template class TicTacToeGame<3, TICTACTOE3_SHAPE_PACK>;
template class TicTacToeGame<4, TICTACTOE4_SHAPE_PACK>;
