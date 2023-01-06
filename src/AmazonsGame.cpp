// AmazonsGame.cpp

#include "AmazonsGame.h"

#include <sstream>

#include "DFAUtil.h"
#include "GameUtil.h"

static dfa_shape_t build_shape(int width, int height)
{
  return dfa_shape_t(width * height, 4);
}

AmazonsGame::AmazonsGame(int width_in, int height_in)
  : NormalPlayGame("amazons" + std::to_string(width_in) + "x" + std::to_string(height_in),
		   build_shape(width_in, height_in)),
    width(width_in),
    height(height_in)
{
}

MoveGraph AmazonsGame::build_move_graph(int side_to_move) const
{
  MoveGraph move_graph;
  move_graph.add_node("begin");
  for(int layer = 0; layer < width * height; ++layer)
    {
      move_graph.add_node("move to=" + std::to_string(layer));
    }
  move_graph.add_node("end");

  // first phase - move a queen

  for(auto& move : GameUtil::get_queen_moves(0, width, height))
    {
      int from_layer = std::get<0>(move);
      int to_layer = std::get<1>(move);
      const std::vector<int>& between_layers = std::get<2>(move);

      choice_type choice = GameUtil::get_choice("move from=" + std::to_string(from_layer) + " to=" + std::to_string(to_layer), 0, width, height);

      // queen leaves at from square
      GameUtil::update_choice(get_shape(), choice, from_layer, 1 + side_to_move, 0, true);

      // squares between from and to squares must be blank
      for(int layer : between_layers)
	{
	  GameUtil::update_choice(get_shape(), choice, layer, 0, 0, true);
	}

      // queen arrives at to square
      GameUtil::update_choice(get_shape(), choice, to_layer, 0, 1 + side_to_move, true);

      // record edge
      move_graph.add_edge(std::get<3>(choice),
			  "begin",
			  "move to=" + std::to_string(to_layer),
			  std::get<0>(choice),
			  std::get<1>(choice),
			  std::get<2>(choice));
    }

  // second phase - shoot an arrow

  for(auto& shot : GameUtil::get_queen_moves(0, width, height))
    {
      int from_layer = std::get<0>(shot);
      int to_layer = std::get<1>(shot);
      const std::vector<int>& between_layers = std::get<2>(shot);

      choice_type choice = GameUtil::get_choice("shot from=" + std::to_string(from_layer) + " to=" + std::to_string(to_layer), 0, width, height);

      // confirm from square is occupied by a friendly queen

      GameUtil::update_choice(get_shape(), choice, from_layer, 1 + side_to_move, 1 + side_to_move, true);

      // squares between from and to squares must be blank
      for(int layer : between_layers)
	{
	  GameUtil::update_choice(get_shape(), choice, layer, 0, 0, true);
	}

      // arrow arrives at to square
      GameUtil::update_choice(get_shape(), choice, to_layer, 0, 3, false);

      // record edge
      move_graph.add_edge(std::get<3>(choice),
			  "move to=" + std::to_string(from_layer),
			  "end",
			  std::get<0>(choice),
			  std::get<1>(choice),
			  std::get<2>(choice));
    }

  return move_graph;
}

shared_dfa_ptr AmazonsGame::get_positions_initial() const
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

  // convert to DFA

  DFAString string(get_shape(), initial_characters);
  return DFAUtil::from_string(string);
}

std::string AmazonsGame::position_to_string(const DFAString& string_in) const
{
  // make sure this is not in the middle of a turn
  assert(string_in[0] == 0);

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
