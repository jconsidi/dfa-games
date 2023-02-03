// OthelloGame.cpp

#include "OthelloGame.h"

#include <stdexcept>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "DFAUtil.h"
#include "GameUtil.h"

#define ASSERT_SIDE_TO_MOVE(side_to_move) assert((0 <= (side_to_move)) && ((side_to_move) <= 1))
#define ASSERT_X(x) assert((0 <= x) && (x < this->width))
#define ASSERT_Y(y) assert((0 <= y) && (y < this->height))

#define CALCULATE_LAYER(x, y) ((x) + this->width * (y))

static std::vector<std::pair<int, int>> directions({{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}});

OthelloGame::OthelloGame(int width_in, int height_in)
  : Game("othello_" + std::to_string(width_in) + "x" + std::to_string(height_in),
	 dfa_shape_t(width_in * height_in, 3)),
    width(width_in),
    height(height_in)
{
  if(width % 2 != 0)
    {
      throw std::logic_error("othello board width must be even");
    }
  if(height % 2 != 0)
    {
      throw std::logic_error("othello board height must be even");
    }
}

MoveGraph OthelloGame::build_move_graph(int side_to_move) const
{
  MoveGraph move_graph(get_shape());

  move_graph.add_node("begin");

  std::vector<std::string> move_end_names;

  // loop over all x,y choices to place a piece
  for(int x_begin = 0; x_begin < width; ++x_begin)
    {
      for(int y_begin = 0; y_begin < height; ++y_begin)
	{
	  // add node beginning this move by placing the piece

	  std::string move_begin_name = get_name_move(x_begin, y_begin) + " begin";

	  change_vector begin_changes(width * height);
	  begin_changes[CALCULATE_LAYER(x_begin, y_begin)] = change_type(0, 1 + side_to_move);

	  std::vector<shared_dfa_ptr> begin_pre_conditions({
	      get_positions_can_place(side_to_move, x_begin, y_begin)
	    });

	  std::vector<shared_dfa_ptr> begin_post_conditions;

	  move_graph.add_node(move_begin_name,
			      begin_changes,
			      begin_pre_conditions,
			      begin_post_conditions);
	  move_graph.add_edge("begin", move_begin_name);

	  std::string previous_name = move_begin_name;

	  // for each direction where flipping is possible, add nodes
	  // for each possible number of flips including zero, and
	  // connect each of those nodes to the previous nodes.
	  for(auto direction : directions)
	    {
	      int x_delta = std::get<0>(direction);
	      int y_delta = std::get<1>(direction);

	      std::vector<std::string> flip_names;

	      std::vector<std::pair<int, int>> between_coordinates;
	      between_coordinates.emplace_back(x_begin + x_delta, y_begin + y_delta);
	      for(int x_end = x_begin + 2 * x_delta,
		    y_end = y_begin + 2 * y_delta;
		  (0 <= x_end) && (x_end < width) && (0 <= y_end) && (y_end < height);
		  x_end += x_delta, y_end += y_delta)
		{
		  std::string flip_name = get_name_flip(x_begin, y_begin, x_end, y_end);

		  change_vector flip_changes(width * height);

		  // fixed beginning
		  flip_changes[CALCULATE_LAYER(x_begin, y_begin)] = change_type(1 + side_to_move, 1 + side_to_move);

		  // flip between
		  for(auto between_coordinate : between_coordinates)
		    {
		      int x_between = std::get<0>(between_coordinate);
		      int y_between = std::get<1>(between_coordinate);

		      flip_changes[CALCULATE_LAYER(x_between, y_between)] = change_type(2 - side_to_move, 1 + side_to_move);
		    }

		  // fixed end
		  flip_changes[CALCULATE_LAYER(x_end, y_end)] = change_type(1 + side_to_move, 1 + side_to_move);

		  // add node

		  move_graph.add_node(flip_name, flip_changes);
		  flip_names.push_back(flip_name);
		}
	      if(flip_names.size() == 0)
		{
		  // no possible flips in this direction
		  continue;
		}

	      // zero flip case
	      std::string zero_flip_name = get_name_move(x_begin, y_begin) + " no flip x_delta=" + std::to_string(x_delta) + ",y_delta=" + std::to_string(y_delta);

	      std::vector<shared_dfa_ptr> flip_conditions;
	      for(auto flip_name : flip_names)
		{
		  flip_conditions.push_back(DFAUtil::get_intersection_vector(get_shape(), move_graph.get_node_pre_conditions(flip_name)));
		}
	      shared_dfa_ptr any_flip_condition = DFAUtil::get_union_vector(get_shape(), flip_conditions);
	      shared_dfa_ptr zero_flip_condition = DFAUtil::get_inverse(any_flip_condition);
	      move_edge_condition_vector zero_flip_conditions({zero_flip_condition});

	      move_graph.add_node(zero_flip_name,
				  change_vector(width * height),
				  zero_flip_conditions,
				  zero_flip_conditions);
	      flip_names.push_back(zero_flip_name);

	      // node aggregating flip choices

	      std::string direction_name = get_name_move(x_begin, y_begin) + " x_delta=" + std::to_string(x_delta) + ",y_delta=" + std::to_string(y_delta);
	      move_graph.add_node(direction_name,
				  CALCULATE_LAYER(x_begin, y_begin),
				  1 + side_to_move,
				  1 + side_to_move);

	      // add edges to/from flip nodes

	      for(auto flip_name : flip_names)
		{
		  move_graph.add_edge(previous_name, flip_name);
		  move_graph.add_edge(flip_name, direction_name);
		}

	      // done with this direction

	      previous_name = direction_name;
	    }

	  move_end_names.push_back(previous_name);
	}
    }

  move_graph.add_node("end");
  for(auto move_end_name : move_end_names)
    {
      move_graph.add_edge(move_end_name, "end");
    }

  // add pass moves

  shared_dfa_ptr pass_condition = DFAUtil::get_difference(get_positions_can_place(1 - side_to_move),
							  get_positions_can_place(side_to_move));

  move_graph.add_edge("pass",
		      "begin",
		      "end",
		      pass_condition);

  // done

  return move_graph;
}

std::string OthelloGame::get_name_flip(int x_begin, int y_begin, int x_end, int y_end) const
{
  return "flip x=" + std::to_string(x_begin) + ",y=" + std::to_string(y_begin) + " to x=" + std::to_string(x_end) + ",y=" + std::to_string(y_end);
}

std::string OthelloGame::get_name_move(int x, int y) const
{
  return "move x=" + std::to_string(x) + ",y=" + std::to_string(y);
}

DFAString OthelloGame::get_position_initial() const
{
  std::vector<int> position_raw(width * height, 0);

  int first_x = width / 2 - 1;
  int first_y = height / 2 - 1;

  position_raw[CALCULATE_LAYER(first_x, first_y)] = 2; // white
  position_raw[CALCULATE_LAYER(first_x, first_y + 1)] = 1; // black
  position_raw[CALCULATE_LAYER(first_x + 1, first_y)] = 1; // black
  position_raw[CALCULATE_LAYER(first_x + 1, first_y + 1)] = 2; // white

  return DFAString(get_shape(), position_raw);
}

shared_dfa_ptr OthelloGame::get_positions_can_flip(int side_to_move, int x_begin, int y_begin, int x_end, int y_end) const
{
  // (x_begin, y_begin) = position where piece will be placed
  // (x_end, y_end) = position of existing friendly piece
  //
  // checks if all pieces between those positions are hostile, and end
  // position is friendly. does not check if begin position is filled
  // since this will be used to check before placing that piece.

  ASSERT_SIDE_TO_MOVE(side_to_move);
  ASSERT_X(x_begin);
  ASSERT_Y(y_begin);
  ASSERT_X(x_end);
  ASSERT_Y(y_end);

  std::vector<shared_dfa_ptr> flip_conditions;

  // between checks
  for(auto between : GameUtil::get_between(x_begin, y_begin, x_end, y_end))
    {
      int x_between = std::get<0>(between);
      int y_between = std::get<1>(between);

      flip_conditions.push_back(DFAUtil::get_fixed(get_shape(),
						   CALCULATE_LAYER(x_between, y_between),
						   2 - side_to_move));
    }
  assert(flip_conditions.size() > 0);

  // end checks
  flip_conditions.push_back(DFAUtil::get_fixed(get_shape(),
					       CALCULATE_LAYER(x_end, y_end),
					       1 + side_to_move));

  // combine and done
  return DFAUtil::get_intersection_vector(get_shape(), flip_conditions);
}

shared_dfa_ptr OthelloGame::get_positions_can_place(int side_to_move) const
{
  return load_or_build("can_place,side=" + std::to_string(side_to_move), [=]()
  {
    std::vector<shared_dfa_ptr> place_choices;
    for(int x = 0; x < width; ++x)
      {
	for(int y = 0; y < width; ++y)
	  {
	    place_choices.push_back(get_positions_can_place(side_to_move, x, y));
	  }
      }
    return DFAUtil::get_union_vector(get_shape(), place_choices);
  });
}

shared_dfa_ptr OthelloGame::get_positions_can_place(int side_to_move, int x_begin, int y_begin) const
{
  return load_or_build("can_place,side=" + std::to_string(side_to_move) + ",x=" + std::to_string(x_begin) + ",y=" + std::to_string(y_begin), [&]()
  {
    shared_dfa_ptr empty_condition = DFAUtil::get_fixed(get_shape(),
							CALCULATE_LAYER(x_begin, y_begin),
							0);

    std::vector<shared_dfa_ptr> flip_choices;
    for(auto direction : directions)
      {
	int x_delta = std::get<0>(direction);
	int y_delta = std::get<1>(direction);

	for(int x_end = x_begin + 2 * x_delta,
	      y_end = y_begin + 2 * y_delta;
	    (0 <= x_end) && (x_end < width) && (0 <= y_end) && (y_end < height);
	    x_end += x_delta, y_end += y_delta)
	  {
	    flip_choices.push_back(get_positions_can_flip(side_to_move, x_begin, y_begin, x_end, y_end));
	  }
      }
    assert(flip_choices.size() > 0);

    shared_dfa_ptr flip_condition = DFAUtil::get_union_vector(get_shape(), flip_choices);

    return DFAUtil::get_intersection(empty_condition, flip_condition);
  });
}

shared_dfa_ptr OthelloGame::get_positions_end() const
{
  return load_or_build("end", [=]()
  {
    // game ends if neither side can place a piece
    return DFAUtil::get_inverse(DFAUtil::get_union(get_positions_can_place(0),
						   get_positions_can_place(1)));
  });
}

shared_dfa_ptr OthelloGame::get_positions_lost(int side_to_move) const
{
  return get_positions_won(1 - side_to_move);
}

shared_dfa_ptr OthelloGame::get_positions_won(int side_to_move) const
{
  ASSERT_SIDE_TO_MOVE(side_to_move);

  return load_or_build(get_name_won(side_to_move), [&]()
  {
    int side_not_to_move = 1 - side_to_move;
    shared_dfa_ptr end_positions = get_positions_end();

    std::vector<shared_dfa_ptr> advantage_staging;
    for(int side_to_move_count = 1; side_to_move_count <= width * height; ++side_to_move_count)
      {
	shared_dfa_ptr side_to_move_constraint =
	  DFAUtil::get_count_character(get_shape(),
				       1 + side_to_move,
				       side_to_move_count);

	// constraint other side to have fewer pieces
	shared_dfa_ptr side_not_to_move_constraint =
	  DFAUtil::get_count_character(get_shape(),
				       1 + side_not_to_move,
				       0, side_to_move_count - 1);

	advantage_staging.push_back(DFAUtil::get_intersection(side_to_move_constraint,
							      side_not_to_move_constraint));
      }

    shared_dfa_ptr advantage_positions = DFAUtil::get_union_vector(get_shape(), advantage_staging);

    return DFAUtil::get_intersection(end_positions, advantage_positions);
  });
}

std::string OthelloGame::position_to_string(const DFAString& string_in) const
{
  std::ostringstream output;
  for(int y = 0; y < height; ++y)
    {
      for(int x = 0; x < width; ++x)
	{
	  int layer = CALCULATE_LAYER(x, y);
	  switch(string_in[layer])
	    {
	    case 0:
	      output << ".";
	      break;
	    case 1:
	      output << "b";
	      break;
	    case 2:
	      output << "w";
	      break;
	    }
	}
      output << "\n";
    }

  return output.str();
}
