// TicTacToeGame.cpp

#include "TicTacToeGame.h"

#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"

static dfa_shape_t get_shape(int n)
{
  return std::vector<int>(n * n, 3);
}

TicTacToeGame::TicTacToeGame(int n_in)
  : Game("tictactoe_" + std::to_string(n_in), ::get_shape(n_in)),
    n(n_in)
{
}

DFAString TicTacToeGame::get_position_initial() const
{
  return DFAString(get_shape(), std::vector<int>(get_shape().size(), 0));
}

shared_dfa_ptr TicTacToeGame::get_lost_condition(int side_to_move, int x_start, int y_start, int x_delta, int y_delta) const
{
  shared_dfa_ptr condition = DFAUtil::get_accept(get_shape());

  for(int i = 0; i < n; ++i)
    {
      int x = x_start + x_delta * i;
      int y = y_start + y_delta * i;
      int index = y * n + x;

      shared_dfa_ptr opponent_piece = DFAUtil::get_fixed(get_shape(), index, 2 - side_to_move);
      condition = DFAUtil::get_intersection(condition, opponent_piece);
    }

  return condition;
}

shared_dfa_ptr TicTacToeGame::get_positions_lost(int side_to_move) const
{
  shared_dfa_ptr lost_positions = DFAUtil::get_reject(get_shape());

  // vertical lost conditions
  for(int x = 0; x < n; ++x)
    {
      shared_dfa_ptr lost_vertical = get_lost_condition(side_to_move, x, 0, 0, 1);
      lost_positions = DFAUtil::get_union(lost_positions, lost_vertical);
    }

  // horizontal lost conditions
  for(int y = 0; y < n; ++y)
    {
      shared_dfa_ptr lost_horizontal = get_lost_condition(side_to_move, 0, y, 1, 0);
      lost_positions = DFAUtil::get_union(lost_positions, lost_horizontal);
    }

  // diagonal lost conditions

  shared_dfa_ptr diagonal_plus = get_lost_condition(side_to_move, 0, 0, 1, 1);
  lost_positions = DFAUtil::get_union(lost_positions, diagonal_plus);

  shared_dfa_ptr diagonal_negative = get_lost_condition(side_to_move, 0, n - 1, 1, -1);
  lost_positions = DFAUtil::get_union(lost_positions, diagonal_negative);

  // done

  return lost_positions;
}

shared_dfa_ptr TicTacToeGame::get_positions_won(int side_to_move) const
{
  return DFAUtil::get_reject(get_shape());
}

MoveGraph TicTacToeGame::build_move_graph(int side_to_move) const
{
  shared_dfa_ptr lost_positions = this->get_positions_lost(side_to_move);
  shared_dfa_ptr not_lost_positions = DFAUtil::get_inverse(lost_positions);

  int side_to_move_piece = 1 + side_to_move;

  MoveGraph move_graph(get_shape());
  move_graph.add_node("begin");
  move_graph.add_node("end");

  for(int move_index = 0; move_index < n * n; ++move_index)
    {
      std::vector<shared_dfa_ptr> pre_conditions;
      pre_conditions.push_back(not_lost_positions);
      pre_conditions.push_back(DFAUtil::get_fixed(get_shape(), move_index, 0));

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
      post_conditions.emplace_back(DFAUtil::get_fixed(get_shape(), move_index, 1 + side_to_move));

      move_graph.add_edge("move_index=" + std::to_string(move_index),
			  "begin",
			  "end",
			  pre_conditions,
			  changes,
			  post_conditions);
    }

  return move_graph;
}

std::string TicTacToeGame::position_to_string(const DFAString& position_in) const
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
