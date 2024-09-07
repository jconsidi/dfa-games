// BreakthroughGame.cpp

#include "BreakthroughGame.h"

#include <cassert>
#include <format>
#include <sstream>
#include <string>

#include "DFAUtil.h"

static dfa_shape_t get_breakthrough_shape(int width, int height)
{
  return dfa_shape_t(width * height, 3);
}

BreakthroughBase::BreakthroughBase(std::string name_in, int width_in, int height_in)
  : NormalPlayGame(name_in,
		   get_breakthrough_shape(width_in, height_in)),
    width(width_in),
    height(height_in)
{
  assert(width >= 1);
  assert(height >= 4);
}

MoveGraph BreakthroughBase::build_move_graph(int side_to_move) const
{
  // setup graph and nodes

  MoveGraph move_graph(get_shape());

  // game not done yet

  std::vector<shared_dfa_ptr> not_done_conditions;
  // side not to move did not get to last row
  for(int col = 0; col < width; ++col)
    {
      int row = (side_to_move == 0) ? 0 : height - 1;
      int layer = calculate_layer(row, col);

      not_done_conditions.push_back(DFAUtil::get_count_character(get_shape(),
								 2 - side_to_move,
								 0, 0,
								 layer, layer));
    }

  shared_dfa_ptr not_done = DFAUtil::get_intersection_vector(get_shape(), not_done_conditions);

  // begin nodes
  move_graph.add_node("begin");
  move_graph.add_node("begin+1");
  move_graph.add_edge("begin not lost", "begin", "begin+1", not_done);

  // move nodes

  auto get_from_node = [](int row, int col) {
    return "from " + std::to_string(row) + "," + std::to_string(col);
  };
  auto get_push_node = [](int row, int col) {
    return "push " + std::to_string(row) + "," + std::to_string(col);
  };
  auto get_capture_node = [](int row, int col) {
    return "capture " + std::to_string(row) + "," + std::to_string(col);
  };

  for(int row = 0; row < height; ++row)
    {
      for(int col = 0; col < width; ++col)
      {
	change_vector move_changes(width * height);
	move_changes[calculate_layer(row, col)] = change_type(1 + side_to_move, 0);
	move_graph.add_node(get_from_node(row, col), move_changes);
      }
    }

  for(int row = 0; row < height; ++row)
    {
      for(int col = 0; col < width; ++col)
      {
	change_vector push_changes(width * height);
	push_changes[calculate_layer(row, col)] = change_type(0, 1 + side_to_move);
	move_graph.add_node(get_push_node(row, col), push_changes);

	change_vector capture_changes(width * height);
	capture_changes[calculate_layer(row, col)] = change_type(2 - side_to_move, 1 + side_to_move);
	move_graph.add_node(get_capture_node(row, col), capture_changes);
      }
    }

  // end nodes
  move_graph.add_node("end-1");
  move_graph.add_node("end");
  move_graph.add_edge("end not lost", "end-1", "end", not_done);

  // add moves

  int delta_row = (side_to_move == 0) ? 1 : -1;
  for(int from_row = 0; from_row < height; ++from_row)
    {
      int to_row = from_row + delta_row;
      if((to_row < 0) || (height <= to_row))
	{
	  continue;
	}

      for(int from_column = 0; from_column < width; ++from_column)
	{
	  std::string from_node = get_from_node(from_row, from_column);
	  move_graph.add_edge("begin+1", from_node);

	  move_graph.add_edge(from_node, get_push_node(to_row, from_column));

	  if(from_column > 0)
	    {
	      move_graph.add_edge(from_node, get_capture_node(to_row, from_column - 1));
	      move_graph.add_edge(from_node, get_push_node(to_row, from_column - 1));
	    }
	  if(from_column < width - 1)
	    {
	      move_graph.add_edge(from_node, get_capture_node(to_row, from_column + 1));
	      move_graph.add_edge(from_node, get_push_node(to_row, from_column + 1));
	    }
	}
    }

  for(int row = 0; row < height; ++row)
    {
      for(int col = 0; col < width; ++col)
      {
	move_graph.add_edge(get_push_node(row, col), "end-1");
	move_graph.add_edge(get_capture_node(row, col), "end-1");
      }
    }

  // done

  return move_graph;
}

DFAString BreakthroughBase::get_position_initial() const
{
  std::vector<int> initial_characters(width * height, 0);

  for(int row = 0; row < 2; ++row)
    {
      for(int column = 0; column < width; ++column)
	{
	  initial_characters.at(calculate_layer(row, column)) = 1;
	}
    }

  for(int row = height - 2; row < height; ++row)
    {
      for(int column = 0; column < width; ++column)
	{
	  initial_characters.at(calculate_layer(row, column)) = 2;
	}
    }

  return DFAString(get_shape(), initial_characters);
}

shared_dfa_ptr BreakthroughBase::build_positions_reversed(shared_dfa_ptr positions_in) const
{
  MoveGraph reverse_graph(get_shape());

  reverse_graph.add_node("begin");
  std::string previous_join = "begin";

  auto change_character = [](int c)
  {
    if(c == 0)
      {
        return 0;
      }

    return 3 - c;
  };

  for(int layer_from = 0; layer_from * 2 < get_shape().size(); ++layer_from)
    {
      int layer_to = get_shape().size() - 1 - layer_from;

      std::vector<std::string> current_changes;
      for(int c_from = 0; c_from < 3; ++c_from)
        {
          for(int c_to = 0; c_to < 3; ++c_to)
            {
              std::string node_name = std::format("layers={:d}_{:d},characters={:d}_{:d}", layer_from, layer_to, c_from, c_to);
              change_vector node_changes(width * height);
              node_changes[layer_from] = change_type(c_from, change_character(c_to));
              node_changes[layer_to] = change_type(c_to, change_character(c_from));

              reverse_graph.add_node(node_name, node_changes);
              current_changes.push_back(node_name);
            }
        }

      std::string next_join = std::format("layers={:d}_{:d},done", layer_from, layer_to);
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

std::string BreakthroughBase::position_to_string(const DFAString& string_in) const
{
  std::ostringstream output;

  for(int row = 0; row < height; ++row)
    {
      for(int column = 0; column < width; ++column)
	{
	  switch(string_in[calculate_layer(row, column)])
	    {
	    case 0:
	      output << ".";
	      break;
	    case 1:
	      output << "x";
	      break;
	    case 2:
	      output << "o";
	      break;
	    default:
	      assert(0);
	    }
	}
      output << "\n";
    }

  return output.str();
}

std::vector<DFAString> BreakthroughBase::validate_moves(int side_to_move, DFAString position) const
{
  auto shape = get_shape();

  std::vector<DFAString> output;

  int friendly_char = 1 + side_to_move;
  int hostile_char = 1 + (1 - side_to_move);

  int row_delta = (side_to_move == 0) ? 1 : -1;

  // check if other side won
  if(side_to_move == 0)
    {
      for(int column = 0; column < width; ++column)
	{
	  int layer = calculate_layer(0, column);
	  if(position[layer] == hostile_char)
	    {
	      return output;
	    }
	}
    }
  else
    {
      for(int column = 0; column < width; ++column)
	{
	  int layer = calculate_layer(height - 1, column);
	  if(position[layer] == hostile_char)
	    {
	      return output;
	    }
	}
    }

  // move generation

  for(int row_from = 0; row_from < height; ++row_from)
    {
      for(int col_from = 0; col_from < width; ++col_from)
	{
	  int layer_from = calculate_layer(row_from, col_from);

	  if(position[layer_from] == friendly_char)
	    {
	      int row_to = row_from + row_delta;
	      if((row_to < 0) || (height <= row_to))
		{
		  continue;
		}

	      for(int col_delta = -1; col_delta <= 1; ++col_delta)
		{
		  int col_to = col_from + col_delta;
		  if((col_to < 0) || (width <= col_to))
		    {
		      continue;
		    }

		  int layer_to = calculate_layer(row_to, col_to);
		  if(position[layer_to] == friendly_char)
		    {
		      // cannot capture own pieces
		      continue;
		    }
		  if((col_delta == 0) && (position[layer_to] == hostile_char))
		    {
		      // cannot capture forward
		      continue;
		    }

		  std::vector<int> position_new;
		  for(int layer = 0; layer < shape.size(); ++layer)
		    {
		      if(layer == layer_from)
			{
			  // from square now empty
			  position_new.push_back(0);
			}
		      else if(layer == layer_to)
			{
			  // to square now taken
			  position_new.push_back(friendly_char);
			}
		      else
			{
			  position_new.push_back(position[layer]);
			}
		    }

		  output.emplace_back(shape, position_new);
		}
	    }
	}
    }

  // done

  return output;
}

BreakthroughColumnWiseGame::BreakthroughColumnWiseGame(int width_in, int height_in)
  : BreakthroughBase(std::format("breakthroughcw_{:d}x{:d}", width_in, height_in),
		     width_in,
		     height_in)
{
}

int BreakthroughColumnWiseGame::calculate_layer(int row, int column) const
{
  return column * height + row;
}

BreakthroughRowWiseGame::BreakthroughRowWiseGame(int width_in, int height_in)
  : BreakthroughBase(std::format("breakthrough_{:d}x{:d}", width_in, height_in),
		     width_in,
		     height_in)
{
}

int BreakthroughRowWiseGame::calculate_layer(int row, int column) const
{
  return row * width + column;
}
