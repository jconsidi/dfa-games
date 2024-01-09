// BreakthroughGame.cpp

#include "BreakthroughGame.h"

#include <cassert>
#include <sstream>
#include <string>

#include "DFAUtil.h"

#define CALCULATE_LAYER(r, c) (r * width + c)

static dfa_shape_t get_breakthrough_shape(int width, int height)
{
  return dfa_shape_t(width * height, 3);
}

BreakthroughGame::BreakthroughGame(int width_in, int height_in)
  : NormalPlayGame("breakthrough_" + std::to_string(width_in) + "x" + std::to_string(height_in),
		   get_breakthrough_shape(width_in, height_in)),
    width(width_in),
    height(height_in)
{
  assert(width >= 2);
  assert(height >= 4);
}

MoveGraph BreakthroughGame::build_move_graph(int side_to_move) const
{
  // setup graph and nodes

  MoveGraph move_graph(get_shape());

  // game not done yet

  std::vector<shared_dfa_ptr> not_done_conditions;
  // side not to move did not get to last row
  not_done_conditions.push_back((side_to_move == 0) ?
				DFAUtil::get_count_character(get_shape(), 2, 0, 0, 0, width - 1) :
				DFAUtil::get_count_character(get_shape(), 1, 0, 0, width * (height - 1), width * height - 1));

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
	move_changes[CALCULATE_LAYER(row, col)] = change_type(1 + side_to_move, 0);
	move_graph.add_node(get_from_node(row, col), move_changes);
      }
    }

  for(int row = 0; row < height; ++row)
    {
      for(int col = 0; col < width; ++col)
      {
	change_vector push_changes(width * height);
	push_changes[CALCULATE_LAYER(row, col)] = change_type(0, 1 + side_to_move);
	move_graph.add_node(get_push_node(row, col), push_changes);

	change_vector capture_changes(width * height);
	capture_changes[CALCULATE_LAYER(row, col)] = change_type(2 - side_to_move, 1 + side_to_move);
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

DFAString BreakthroughGame::get_position_initial() const
{
  std::vector<int> initial_characters(width * height, 0);

  for(int row = 0; row < 2; ++row)
    {
      for(int column = 0; column < width; ++column)
	{
	  initial_characters.at(row * width + column) = 1;
	}
    }

  for(int row = height - 2; row < height; ++row)
    {
      for(int column = 0; column < width; ++column)
	{
	  initial_characters.at(row * width + column) = 2;
	}
    }

  return DFAString(get_shape(), initial_characters);
}

std::string BreakthroughGame::position_to_string(const DFAString& string_in) const
{
  std::ostringstream output;

  for(int row = 0; row < height; ++row)
    {
      for(int column = 0; column < width; ++column)
	{
	  switch(string_in[row * width + column])
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

std::vector<DFAString> BreakthroughGame::validate_moves(int side_to_move, DFAString position) const
{
  auto shape = get_shape();

  std::vector<DFAString> output;

  int friendly_char = 1 + side_to_move;
  int hostile_char = 1 + (1 - side_to_move);

  int row_delta = (side_to_move == 0) ? 1 : -1;

  // check if other side won
  if(side_to_move == 0)
    {
      for(int layer = 0; layer < width; ++layer)
	{
	  if(position[layer] == hostile_char)
	    {
	      return output;
	    }
	}
    }
  else
    {
      for(int layer = shape.size() - width; layer < shape.size(); ++layer)
	{
	  if(position[layer] == hostile_char)
	    {
	      return output;
	    }
	}
    }

  // move generation

  for(int layer_from = 0; layer_from < shape.size(); ++layer_from)
    {
      if(position[layer_from] == friendly_char)
	{
	  int row_from = layer_from / width;
	  int col_from = layer_from % width;

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

	      int layer_to = row_to * width + col_to;
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

  // done

  return output;
}
