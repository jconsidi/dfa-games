// BreakthroughGame.cpp

#include "BreakthroughGame.h"

#include <cassert>
#include <format>
#include <sstream>
#include <string>

BreakthroughGame::BreakthroughGame(int width_in, int height_in)
  : RectangularBase(width_in, height_in),
    RowMajorOrderBase(width_in, height_in),
    ConfigNormalPlayGame(std::format("breakthrough_{:d}x{:d}", width_in, height_in))
{
  assert(width >= 1);
  assert(height >= 4);
}

shared_dfa_ptr BreakthroughGame::build_positions_reversed(shared_dfa_ptr positions_in) const
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
      int layer_to = get_shape_size() - 1 - layer_from;

      std::vector<std::string> current_changes;

      if(layer_from < layer_to)
        {
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
        }
      else
        {
          for(int c_from = 0; c_from < 3; ++c_from)
            {
              std::string node_name = std::format("layers={:d},characters={:d}", layer_from, c_from);
              change_vector node_changes(width * height);
              node_changes[layer_from] = change_type(c_from, change_character(c_from));

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

std::string BreakthroughGame::position_to_string(const DFAString& string_in) const
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
	      output << "w";
	      break;
	    case 2:
	      output << "b";
	      break;
	    default:
	      assert(0);
	    }
	}
      output << "\n";
    }

  return output.str();
}

std::vector<DFAString> BreakthroughGame::validate_moves(int side_to_move, const DFAString& position) const
{
  const auto& shape = get_shape();

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
