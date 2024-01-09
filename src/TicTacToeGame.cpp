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

shared_dfa_ptr TicTacToeGame::build_positions_lost(int side_to_move) const
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

MoveGraph TicTacToeGame::build_move_graph(int side_to_move) const
{
  shared_dfa_ptr lost_positions = this->get_positions_lost(side_to_move);
  shared_dfa_ptr not_lost_positions = DFAUtil::get_inverse(lost_positions);

  int side_to_move_piece = 1 + side_to_move;

  std::vector<int> move_layers;
  for(int move_layer = 0; move_layer < n * n; ++move_layer)
    {
      move_layers.push_back(move_layer);
    }
  auto get_move_name = [](int move_layer)
  {
    return "move=" + std::to_string(move_layer);
  };

  MoveGraph move_graph(get_shape());

  // setup nodes
  move_graph.add_node("begin");
  for(int move_layer : move_layers)
    {
      move_graph.add_node(get_move_name(move_layer), move_layer, 0, side_to_move_piece);
    }
  move_graph.add_node("end");

  // setup edges to/from move nodes
  for(int move_layer : move_layers)
    {
      move_graph.add_edge("pre " + get_move_name(move_layer),
			  "begin",
			  get_move_name(move_layer),
			  not_lost_positions);
      move_graph.add_edge("post " + get_move_name(move_layer),
			  get_move_name(move_layer),
			  "end",
			  not_lost_positions);
    }

  // done

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

std::vector<DFAString> TicTacToeGame::validate_moves(int, DFAString) const
{
  throw std::logic_error("not implemented");
}

int TicTacToeGame::validate_result(int, DFAString) const
{
  throw std::logic_error("not implemented");
}
