// AmazonsGame.cpp

#include "AmazonsGame.h"

#include <cstdlib>
#include <format>
#include <sstream>

#include "GameUtil.h"

static std::string build_name(int width, int height)
{
  return std::format("amazons_{:d}x{:d}", width, height);
}

static std::vector<std::vector<std::tuple<int, std::vector<int>>>> build_queen_moves_by_layer(int width, int height)
{
  std::vector<std::vector<std::tuple<int, std::vector<int>>>> output(size_t(width) * size_t(height));

  for(const auto& queen_move : GameUtil::get_queen_moves(0, width, height))
    {
      int from_layer = std::get<0>(queen_move);
      int to_layer = std::get<1>(queen_move);
      const std::vector<int>& between = std::get<2>(queen_move);

      output.at(size_t(from_layer)).emplace_back(to_layer, between);
    }

  return output;
}

AmazonsGame::AmazonsGame(int width_in, int height_in)
  : ConfigNormalPlayGame(build_name(width_in, height_in)),
    width(width_in),
    height(height_in),
    queen_moves_by_layer(build_queen_moves_by_layer(width_in, height_in))
{
}

shared_dfa_ptr AmazonsGame::build_positions_reversed(shared_dfa_ptr positions_in) const
{
  MoveGraph reverse_graph(get_shape());

  reverse_graph.add_node("begin");
  std::string previous_join = "begin";

  for(int layer = 0; layer < get_shape().size(); ++layer)
    {
      std::vector<std::string> current_changes;
      for(int c_from = 0; c_from < 4; ++c_from)
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

std::string AmazonsGame::position_to_string(const DFAString& string_in) const
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
	    case 3:
	      output << "*";
	      break;
	    }
	}
      output << "\n";
    }

  return output.str();
}

std::vector<DFAString> AmazonsGame::validate_moves(int side_to_move, const DFAString& position) const
{
  std::vector<DFAString> output;

  auto check_between = [&](const std::vector<int>& between_layers)
  {
    for(int layer : between_layers)
      {
        if(position[layer])
          {
            return false;
          }
      }

    return true;
  };

  size_t layers = get_shape().size();
  for(int from_layer = 0; from_layer < layers; ++from_layer)
    {
      if(position[from_layer] != 1 + side_to_move)
        {
          // from square not friendly piece
          continue;
        }

      for(const auto& queen_move : queen_moves_by_layer.at(size_t(from_layer)))
        {
          int to_layer = std::get<0>(queen_move);
          if(position[to_layer] != 0)
            {
              // to square not empty
              continue;
            }

          if(!check_between(std::get<1>(queen_move)))
            {
              // not a queen move
              continue;
            }

          for(const auto& queen_shot : queen_moves_by_layer.at(size_t(to_layer)))
            {
              int shot_layer = std::get<0>(queen_shot);
              if((shot_layer != from_layer) && (position[shot_layer] != 0))
                {
                  continue;
                }

              std::vector<int> shot_between;
              for(int shot_between_layer : std::get<1>(queen_shot))
                {
                  if(shot_between_layer != from_layer)
                    {
                      shot_between.push_back(shot_between_layer);
                    }
                }
              if(!check_between(shot_between))
                {
                  continue;
                }

              std::vector<int> move(layers);
              for(int i = 0; i < layers; ++i)
                {
                  move[i] = position[i];
                }
              move[from_layer] = 0;
              move[to_layer] = 1 + side_to_move;
              move[shot_layer] = 3;
              output.emplace_back(get_shape(), move);
            }
        }
    }

  return output;
}
