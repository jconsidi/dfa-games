// AmazonsGame.cpp

#include "AmazonsGame.h"

#include <cassert>
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
  move_graph.add_node("begin+1");
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
  move_graph.add_node("end-1");
  move_graph.add_node("end");

  // pre and post legal conditions

  shared_dfa_ptr accept = DFAUtil::get_accept(get_shape());

  shared_dfa_ptr legal_condition = accept;

  // begin - legal positions

  move_graph.add_edge("begin legal",
		      "begin",
		      "begin+1",
		      legal_condition);

  // pick queen to move

  for(int from_layer : layers)
    {
      move_graph.add_edge(get_node_name("move from", from_layer),
			  "begin+1",
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
			  "end-1");
    }

  // end - legal positions

  move_graph.add_edge("end legal",
		      "end-1",
		      "end",
		      legal_condition);

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

std::vector<DFAString> AmazonsGame::validate_moves(int, DFAString) const
{
  throw std::logic_error("not implemented");
}
