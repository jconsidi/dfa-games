// TicTacToeGame.cpp

#include "TicTacToeGame.h"

#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"

TicTacToeGame::TicTacToeGame(int n_in)
  : ConfigGame("tictactoe_" + std::to_string(n_in)),
    n(n_in)
{
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
