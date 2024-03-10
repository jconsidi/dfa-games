// AmazonsGame.cpp

#include "AmazonsGame.h"

#include <cassert>
#include <cstdlib>
#include <sstream>

#include "DFAUtil.h"
#include "GameUtil.h"

static dfa_shape_t build_shape(int width, int height)
{
  return dfa_shape_t(width * height, 4);
}

static move_edge_condition_vector get_empty_conditions(dfa_shape_t shape, const std::vector<int> empty_layers)
{
  move_edge_condition_vector output;
  for(int empty_layer : empty_layers)
    {
      output.push_back(DFAUtil::get_fixed(shape, empty_layer, 0));
    }

  return output;
}

static std::string get_node_name(std::string node_type, int layer)
{
  return node_type + "=" + std::to_string(layer);
}

AmazonsGame::AmazonsGame(int width_in, int height_in)
  : NormalPlayGame("amazons_" + std::to_string(width_in) + "x" + std::to_string(height_in),
		   build_shape(width_in, height_in)),
    width(width_in),
    height(height_in)
{
}

MoveGraph AmazonsGame::build_move_graph(int side_to_move) const
{
  // useful iterables

  std::vector<int> layers;
  for(int layer = 0; layer < width * height; ++layer)
    {
      layers.push_back(layer);
    }

  // setup graph and nodes

  MoveGraph move_graph(get_shape());
  move_graph.add_node("begin");
  for(int layer : layers)
    {
      move_graph.add_node(get_node_name("move from", layer), layer, 1 + side_to_move, 0);
    }
  for(int layer : layers)
    {
      move_graph.add_node(get_node_name("move to", layer), layer, 0, 1 + side_to_move);
    }
  for(int layer : layers)
    {
      move_graph.add_node(get_node_name("shot at", layer), layer, 0, 3);
    }
  move_graph.add_node("end");

  // pick queen to move

  for(int from_layer : layers)
    {
      move_graph.add_edge(get_node_name("move from", from_layer),
			  "begin",
			  get_node_name("move from", from_layer));
    }

  // picked queen moves

  for(auto& move : GameUtil::get_queen_moves(0, width, height))
    {
      int from_layer = std::get<0>(move);
      int to_layer = std::get<1>(move);
      const std::vector<int>& between_layers = std::get<2>(move);

      move_graph.add_edge(get_node_name("move from", from_layer) + ", " + get_node_name("move to", to_layer),
			  get_node_name("move from", from_layer),
			  get_node_name("move to", to_layer),
			  get_empty_conditions(get_shape(), between_layers));
    }

  // moved queen shoots

  for(auto& shot : GameUtil::get_queen_moves(0, width, height))
    {
      int to_layer = std::get<0>(shot);
      int at_layer = std::get<1>(shot);
      const std::vector<int>& between_layers = std::get<2>(shot);

      move_graph.add_edge(get_node_name("move to", to_layer) + ", " + get_node_name("shot at", at_layer),
			  get_node_name("move to", to_layer),
			  get_node_name("shot at", at_layer),
			  get_empty_conditions(get_shape(), between_layers));
    }

  // shot choice

  for(int at_layer = 0; at_layer < width * height; ++at_layer)
    {
      move_graph.add_edge(get_node_name("shot at", at_layer),
			  get_node_name("shot at", at_layer),
			  "end");
    }

  // done

  return move_graph;
}

DFAString AmazonsGame::get_position_initial() const
{
  std::vector<int> initial_characters(width * height, 0);

  int smaller_dim = (width < height) ? width : height;

  int corner_offset = 0;
  switch(smaller_dim)
    {
    case 4:
    case 5:
    case 6:
    case 7:
      corner_offset = 1;
      break;
    case 8:
    case 9:
      corner_offset = 2;
      break;
    case 10:
      corner_offset = 3;
      break;
    }
  assert(corner_offset > 0);

  auto set_square = [&](int x, int y, int character)
  {
    int square = x + width * y;
    int layer = square + 0;

    initial_characters[layer] = character;
  };

  // top row - first player
  set_square(corner_offset, 0, 1);
  set_square(width - 1 - corner_offset, 0, 1);
  // middle row - first player
  set_square(0, corner_offset, 1);
  set_square(width - 1, corner_offset, 1);

  // middle row - second player
  set_square(0, height - 1 - corner_offset, 2);
  set_square(width - 1, height - 1 - corner_offset, 2);

  // bottom row - second player
  set_square(corner_offset, height - 1, 2);
  set_square(width - 1 - corner_offset, height - 1, 2);

  // convert to DFAString

  return DFAString(get_shape(), initial_characters);
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

std::vector<DFAString> AmazonsGame::validate_moves(int side_to_move, DFAString position) const
{
  std::vector<DFAString> output;

  std::vector<std::vector<std::tuple<int, std::vector<int>>>> queen_moves(get_shape().size());
  for(auto queen_move : GameUtil::get_queen_moves(0, width, height))
    {
      int from_layer = std::get<0>(queen_move);
      int to_layer = std::get<1>(queen_move);
      const std::vector<int>& between = std::get<2>(queen_move);

      queen_moves[from_layer].emplace_back(to_layer, between);
    }

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

  int layers = get_shape().size();
  for(int from_layer = 0; from_layer < layers; ++from_layer)
    {
      if(position[from_layer] != 1 + side_to_move)
        {
          // from square not friendly piece
          continue;
        }

      for(auto queen_move : queen_moves.at(from_layer))
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

          for(auto queen_shot : queen_moves.at(to_layer))
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
