// ClobberGame.cpp

#include "ClobberGame.h"

#include <cassert>
#include <cstdlib>
#include <format>
#include <sstream>

#include "DFAUtil.h"
#include "GameUtil.h"

static dfa_shape_t build_shape(int width, int height)
{
  return dfa_shape_t(width * height, 3);
}

ClobberGame::ClobberGame(int width_in, int height_in)
  : NormalPlayGame("clobber_" + std::to_string(width_in) + "x" + std::to_string(height_in),
		   build_shape(width_in, height_in)),
    width(width_in),
    height(height_in)
{
}

MoveGraph ClobberGame::build_move_graph(int side_to_move) const
{
  // setup graph and nodes

  MoveGraph move_graph(get_shape());
  move_graph.add_node("begin");

  std::vector<std::string> move_names;

  for(int x0 = 0; x0 < width; ++x0)
    {
      for(int y0 = 0; y0 < height; ++y0)
        {
          auto add_move = [&](int x1, int y1)
          {
            std::string move_name = "from " + std::to_string(x0) + "," + std::to_string(y0) + " to " + std::to_string(x1) + "," + std::to_string(y1);
            move_names.push_back(move_name);

            int l0 = x0 + width * y0;
            int l1 = x1 + width * y1;

            change_vector move_changes(width * height);
            move_changes[l0] = change_type(1 + side_to_move, 0);
            move_changes[l1] = change_type(2 - side_to_move, 1 + side_to_move);

            move_graph.add_node(move_name, move_changes);
          };
          
          if(x0 > 0)
            {
              add_move(x0 - 1, y0);
            }
          if(x0 + 1 < width)
            {
              add_move(x0 + 1, y0);
            }
          if(y0 > 0)
            {
              add_move(x0, y0 - 1);
            }
          if(y0 + 1 < height)
            {
              add_move(x0, y0 + 1);
            }
        }
    }

  move_graph.add_node("end");

  for(auto move_name : move_names)
    {
      move_graph.add_edge("begin", move_name);
      move_graph.add_edge(move_name, "end");
    }

  // done

  return move_graph;
}

#if 0
shared_dfa_ptr ClobberGame::build_positions_reversed(shared_dfa_ptr positions_in) const
{
  MoveGraph reverse_graph(get_shape());

  reverse_graph.add_node("begin");
  std::string previous_join = "begin";

  for(int layer = 0; layer < get_shape().size(); ++layer)
    {
      std::vector<std::string> current_changes;
      for(int c_from = 0; c_from < 3; ++c_from)
        {
          int c_to = ((c_from == 1) || (c_from == 2)) ? (3 - c_from) : c_from;

          std::string node_name = std::format("layer={:d},c={:d}", layer, c_from);
          change_vector node_changes(width * height);
          node_changes[layer] = change_type(c_from, c_to);

          reverse_graph.add_node(node_name, node_changes);
          current_changes.push_back(node_name);
        }

      std::string next_join = std::format("layers={:d} done", layer);
      reverse_graph.add_node(next_join);
      for(std::string node_name : current_changes)
        {
          reverse_graph.add_edge(previous_join, node_name);
          reverse_graph.add_edge(node_name, next_join);
        }
      previous_join = next_join;
    }

  std::string name_prefix = std::format("{:s},reversed", get_name());
  return reverse_graph.get_moves(name_prefix, positions_in);
}
#endif

DFAString ClobberGame::get_position_initial() const
{
  std::vector<int> initial_characters(width * height, 0);

  for(int layer = 0; layer < width * height; ++layer)
    {
      int x = layer % width;
      int y = layer / width;

      initial_characters[layer] = 1 + (x + y) % 2;
    }

  return DFAString(get_shape(), initial_characters);
}

std::string ClobberGame::position_to_string(const DFAString& string_in) const
{
  std::ostringstream output;
  for(int y = height - 1; y >= 0; --y)
    {
      for(int x = 0; x < width; ++x)
	{
	  int square = x + width * y;
	  int layer = square + 0;
	  switch(string_in[layer])
	    {
	    case 0:
	      output << ".";
	      break;
	    case 1:
	      output << "w";
	      break;
	    case 2:
	      output << "b";
	      break;
	    }
	}
      output << "\n";
    }

  return output.str();
}
