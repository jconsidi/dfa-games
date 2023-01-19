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
  : NormalPlayGame("amazons_" + std::to_string(width_in) + "x" + std::to_string(height_in),
		   build_shape(width_in, height_in)),
    width(width_in),
    height(height_in)
{
}

MoveGraph AmazonsGame::build_move_graph(int side_to_move) const
{
  MoveGraph move_graph(get_shape());
  move_graph.add_node("begin");
  move_graph.add_node("pre legal");
  for(int layer = 0; layer < width * height; ++layer)
    {
      move_graph.add_node("move from=" + std::to_string(layer));
    }
  for(int layer = 0; layer < width * height; ++layer)
    {
      move_graph.add_node("move to=" + std::to_string(layer));
    }
  for(int layer = 0; layer < width * height; ++layer)
    {
      move_graph.add_node("shot at=" + std::to_string(layer));
    }
  move_graph.add_node("post legal");
  move_graph.add_node("end");

  // pre and post legal conditions

  shared_dfa_ptr accept = DFAUtil::get_accept(get_shape());
  const change_vector change_nop(width * height);

  shared_dfa_ptr legal_condition = accept;
  for(int queen_character = 1; queen_character < 3; ++queen_character)
    {
      // 4 queens on each side
      shared_dfa_ptr count_condition = DFAUtil::get_count_character(get_shape(), queen_character, 4, 4, 1);
      legal_condition = DFAUtil::get_intersection(legal_condition, count_condition);
    }
  legal_condition->set_name("legal_condition");

  move_graph.add_edge("pre legal",
		      "begin",
		      "pre legal",
		      move_edge_condition_vector({legal_condition}),
		      change_nop,
		      move_edge_condition_vector({accept}));

  move_graph.add_edge("post legal",
		      "post legal",
		      "end",
		      move_edge_condition_vector({accept}),
		      change_nop,
		      move_edge_condition_vector({legal_condition}));

  // pick queen to move

  for(int from_layer = 0; from_layer < width * height; ++from_layer)
    {
      move_graph.add_edge("move from=" + std::to_string(from_layer),
			  "pre legal",
			  "move from=" + std::to_string(from_layer),
			  from_layer,
			  change_type(1 + side_to_move, 0));
    }

  // picked queen moves

  for(auto& move : GameUtil::get_queen_moves(0, width, height))
    {
      int from_layer = std::get<0>(move);
      int to_layer = std::get<1>(move);
      const std::vector<int>& between_layers = std::get<2>(move);

      choice_type choice = GameUtil::get_choice("move from=" + std::to_string(from_layer) + " to=" + std::to_string(to_layer), 0, width, height);

      // queen already left from square
      GameUtil::update_choice(get_shape(), choice, from_layer, 0, 0, true);

      // squares between from and to squares must be blank
      for(int layer : between_layers)
	{
	  GameUtil::update_choice(get_shape(), choice, layer, 0, 0, true);
	}

      // queen arrives at to square
      GameUtil::update_choice(get_shape(), choice, to_layer, 0, 1 + side_to_move, true);

      // record edge
      move_graph.add_edge(std::get<3>(choice),
			  "move from=" + std::to_string(from_layer),
			  "move to=" + std::to_string(to_layer),
			  std::get<0>(choice),
			  std::get<1>(choice),
			  std::get<2>(choice));
    }

  // moved queen shoots

  for(auto& shot : GameUtil::get_queen_moves(0, width, height))
    {
      int to_layer = std::get<0>(shot);
      int at_layer = std::get<1>(shot);
      const std::vector<int>& between_layers = std::get<2>(shot);

      choice_type choice = GameUtil::get_choice("", 0, width, height);

      // confirm move to square is occupied by a friendly queen

      GameUtil::update_choice(get_shape(), choice, to_layer, 1 + side_to_move, 1 + side_to_move, true);

      // squares between from and to squares must be blank
      for(int layer : between_layers)
	{
	  GameUtil::update_choice(get_shape(), choice, layer, 0, 0, true);
	}

      // arrow aimed at empty square
      GameUtil::update_choice(get_shape(), choice, at_layer, 0, 0, false);

      // record edge
      move_graph.add_edge("move to=" + std::to_string(to_layer) + " shot at=" + std::to_string(at_layer),
			  "move to=" + std::to_string(to_layer),
			  "shot at=" + std::to_string(at_layer),
			  std::get<0>(choice),
			  std::get<1>(choice),
			  std::get<2>(choice));
    }

  // shot lands at empty square

  for(int at_layer = 0; at_layer < width * height; ++at_layer)
    {
      move_graph.add_edge("shot at=" + std::to_string(at_layer),
			  "shot at=" + std::to_string(at_layer),
			  "post legal",
			  at_layer,
			  change_type(0, 3));
    }

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
