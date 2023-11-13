// ChessGame.cpp

#include "ChessGame.h"

#include <bit>
#include <functional>
#include <iostream>
#include <string>

#include "BetweenMasks.h"
#include "CountCharacterDFA.h"
#include "DFAUtil.h"
#include "Profile.h"

static const dfa_shape_t chess_shape = {CHESS_DFA_SHAPE};

static bool chess_is_friendly(int side_to_move, int character)
{
  if(character == DFA_BLANK)
    {
      return false;
    }

  if(side_to_move == SIDE_WHITE)
    {
      return character % 2 == 1;
    }
  else
    {
      return character % 2 == 0;
    }
}

static bool chess_is_hostile(int side_to_move, int character)
{
  return chess_is_friendly(1 - side_to_move, character);
}

ChessGame::ChessGame()
  : Game("chess+" + std::to_string(CHESS_SQUARE_OFFSET), chess_shape)
{
}

int ChessGame::_calc_castle_rank(int side) const
{
  return (side == SIDE_WHITE) ? 7 : 0;
}

int ChessGame::_calc_layer(int rank, int file) const
{
  return _calc_square(rank, file) + CHESS_SQUARE_OFFSET;
}

int ChessGame::_calc_square(int rank, int file) const
{
  return rank * 8 + file;
}

change_vector ChessGame::_get_changes() const
{
  return change_vector(get_shape().size());
}

std::vector<std::tuple<int, int, int, int, std::vector<int>, bool>> ChessGame::_get_choices_basic(int side_to_move) const
{
  // output columns
  // 0: from_character
  // 1: from_square
  // 2: to_character
  // 3: to_square
  // 4: empty_squares
  // 5: capture_only

  std::vector<std::tuple<int, int, int, int, std::vector<int>, bool>> output;

  _get_choices_basic_helper(output, side_to_move, DFA_WHITE_KING, DFA_WHITE_KING, king_moves);
  _get_choices_basic_helper(output, side_to_move, DFA_WHITE_BISHOP, DFA_WHITE_BISHOP, bishop_moves);
  _get_choices_basic_helper(output, side_to_move, DFA_WHITE_KNIGHT, DFA_WHITE_KNIGHT, knight_moves);
  _get_choices_basic_helper(output, side_to_move, DFA_WHITE_QUEEN, DFA_WHITE_QUEEN, queen_moves);
  _get_choices_basic_helper(output, side_to_move, DFA_WHITE_ROOK, DFA_WHITE_ROOK, rook_moves);
  _get_choices_basic_helper(output, side_to_move, DFA_WHITE_ROOK_CASTLE, DFA_WHITE_ROOK, rook_moves);

  _get_choices_basic_pawns(output, side_to_move);

  return output;
}

void ChessGame::_get_choices_basic_helper(std::vector<std::tuple<int, int, int, int, std::vector<int>, bool>>& output,
					  int side_to_move,
					  int from_character_white,
					  int to_character_white,
					  const MoveSet& moves) const
{
  int from_character = from_character_white + side_to_move;
  int to_character = to_character_white + side_to_move;

  for(int from_square = 0; from_square < 64; ++from_square)
    {
      int from_layer = from_square + CHESS_SQUARE_OFFSET;
      int from_layer_shape = get_shape()[from_layer];
      if(from_character >= from_layer_shape)
	{
	  continue;
	}

      switch(from_character)
	{
	case DFA_BLACK_ROOK_CASTLE:
	  if((from_square != 0) && (from_square != 7))
	    {
	      // black rook with castle privileges can only be in two positions.
	      continue;
	    }
	  break;

	case DFA_WHITE_ROOK_CASTLE:
	  if((from_square != 56) && (from_square != 63))
	    {
	      // white rook with castle privileges can only be in two positions.
	      continue;
	    }
	  break;
	}

      for(int to_square = 0; to_square < 64; ++to_square)
	{
	  if(!(moves.moves[from_square] & (1ULL << to_square)))
	    {
	      continue;
	    }
	  assert(to_square != from_square);

	  output.emplace_back(from_character,
			      from_square,
			      to_character,
			      to_square,
			      _get_squares_between(from_square, to_square),
			      false);
	}
    }
}

void ChessGame::_get_choices_basic_pawns(std::vector<std::tuple<int, int, int, int, std::vector<int>, bool>>& output,
					 int side_to_move) const
{
  int pawn_character = DFA_WHITE_PAWN + side_to_move;
  int en_passant_character = DFA_WHITE_PAWN_EN_PASSANT + side_to_move;

  int pawn_rank_start = (side_to_move == SIDE_WHITE) ? 6 : 1;
  int pawn_direction = (side_to_move == SIDE_WHITE) ? -1 : 1;

  for(int from_file = 0; from_file < 8; ++from_file)
    {
      for(int previous_advancement = 0; previous_advancement < 6; ++previous_advancement)
	{
	  int from_rank = pawn_rank_start + previous_advancement * pawn_direction;
	  int from_square = _calc_square(from_rank, from_file);
	  int advance_rank = from_rank + pawn_direction;

	  std::vector<int> advance_characters;
	  if(previous_advancement < 5)
	    {
	      advance_characters.push_back(pawn_character);
	    }
	  else
	    {
	      for(int advance_character = DFA_WHITE_QUEEN + side_to_move;
		  advance_character < DFA_WHITE_PAWN;
		  advance_character += 2)
		{
		  advance_characters.push_back(advance_character);
		}
	    }

	  for(int advance_character : advance_characters)
	    {
	      // single advance
	      int advance_square = _calc_square(advance_rank, from_file);
	      output.emplace_back(pawn_character,
				  from_square,
				  advance_character,
				  advance_square,
				  std::vector<int>({advance_square}),
				  false);

	      for(int capture_file = from_file - 1;
		  capture_file <= from_file + 1;
		  capture_file += 2)
		{
		  if((capture_file < 0) || (8 <= capture_file))
		    {
		      continue;
		    }

		  int capture_square = _calc_square(advance_rank, capture_file);
		  output.emplace_back(pawn_character,
				      from_square,
				      advance_character,
				      capture_square,
				      std::vector<int>(),
				      true);
		}
	    }
	}

      // double advance

      output.emplace_back(pawn_character,
			  _calc_square(pawn_rank_start, from_file),
			  en_passant_character,
			  _calc_square(pawn_rank_start + 2 * pawn_direction, from_file),
			  std::vector<int>({
			      _calc_square(pawn_rank_start + 1 * pawn_direction, from_file),
			      _calc_square(pawn_rank_start + 2 * pawn_direction, from_file)
			    }),
			  false);
    }
}

std::vector<std::pair<int, int>> ChessGame::_get_choices_from(int side_to_move) const
{
  std::vector<std::pair<int, int>> output;

  for(int from_character = 1 + side_to_move;
      from_character < DFA_MAX;
      from_character += 2)
    {
      int from_square_min = 0;
      int from_square_max = 63;
      int from_square_step = 1;
      switch(from_character)
	{
	case DFA_WHITE_PAWN:
	case DFA_BLACK_PAWN:
	  from_square_min = 8;
	  from_square_max = 55;
	  break;

	case DFA_WHITE_PAWN_EN_PASSANT:
	case DFA_BLACK_PAWN_EN_PASSANT:
	  // this status is cleared on the previous turn
	  continue;

	case DFA_WHITE_ROOK_CASTLE:
	  from_square_min = 56;
	  from_square_max = 63;
	  from_square_step = 7;
	  break;

	case DFA_BLACK_ROOK_CASTLE:
	  from_square_min = 0;
	  from_square_max = 7;
	  from_square_step = 7;
	  break;
	}

      for(int from_square = from_square_min;
	  from_square <= from_square_max;
	  from_square += from_square_step)
	{
	  output.emplace_back(from_character, from_square);
	}
    }

  return output;
}

std::vector<std::pair<int, int>> ChessGame::_get_choices_to(int side_to_move) const
{
  std::vector<std::pair<int, int>> output;

  for(int to_character = 1 + side_to_move;
      to_character <= DFA_BLACK_PAWN_EN_PASSANT;
      to_character += 2)
    {
      int to_square_min = 0;
      int to_square_max = 63;
      switch(to_character)
	{
	case DFA_BLACK_PAWN:
	  to_square_min = 16;
	  to_square_max = 55;
	  break;

	case DFA_WHITE_PAWN:
	  to_square_min = 8;
	  to_square_max = 47;
	  break;
	}

      for(int to_square = to_square_min; to_square <= to_square_max; ++to_square)
       {
         output.emplace_back(to_character, to_square);
       }
    }

  return output;
}

std::vector<std::tuple<int, int, int>> ChessGame::_get_choices_to_prev(int side_to_move) const
{
  std::vector<std::tuple<int, int, int>> output;

  std::vector<int> previous_characters;
  previous_characters.push_back(DFA_BLANK);
  for(int previous_character = DFA_WHITE_KING + (1 - side_to_move);
      previous_character < DFA_MAX;
      previous_character += 2)
    {
      previous_characters.push_back(previous_character);
    }

  for(auto to_choice : _get_choices_to(side_to_move))
    {
      int to_character = std::get<0>(to_choice);
      int to_square = std::get<1>(to_choice);

      int to_layer = to_square + CHESS_SQUARE_OFFSET;
      int to_layer_shape = get_shape().at(to_layer);

      for(int previous_character : previous_characters)
	{
	  if(previous_character < to_layer_shape)
	    {
	      output.emplace_back(to_character, to_square, previous_character);
	    }
	}
    }

  return output;
}

std::string ChessGame::_get_name_from(int from_character, int from_square) const
{
  return "from_char=" + std::to_string(from_character) + ", from_square=" + std::to_string(from_square);
}

std::string ChessGame::_get_name_to(int to_character, int to_square) const
{
  return "to_char=" + std::to_string(to_character) + ", to_square=" + std::to_string(to_square);
}

std::string ChessGame::_get_name_to_prev(int to_character, int to_square, int prev_character) const
{
  return _get_name_to(to_character, to_square) + ", prev=" + std::to_string(prev_character);
}

std::vector<int> ChessGame::_get_squares_between(int from_square, int to_square) const
{
  assert((0 <= from_square) && (from_square < 64));
  assert((0 <= to_square) && (to_square < 64));

  std::vector<int> output;

  BoardMask between_mask = between_masks.masks[from_square][to_square];

  auto check_square =
    [&](int square)
    {
      if(between_mask & (1ULL << square))
	{
	  output.push_back(square);
	}
    };

  // add constraints starting on from side
  if(from_square < to_square)
    {
      for(int square = 0; square < 64; ++square)
	{
	  check_square(square);
	}
    }
  else
    {
      for(int square = 63; square >= 0; --square)
	{
	  check_square(square);
	}
    }

  return output;
}

MoveGraph ChessGame::build_move_graph(int side_to_move) const
{
  assert((0 <= side_to_move) && (side_to_move <= 1));

  // side to move characters

  //int bishop_character = DFA_WHITE_BISHOP + side_to_move;
  int king_character = DFA_WHITE_KING + side_to_move;
  //int knight_character = DFA_WHITE_KNIGHT + side_to_move;
  int pawn_character = DFA_WHITE_PAWN + side_to_move;
  //int pawn_en_passant_character = DFA_WHITE_PAWN_EN_PASSANT + side_to_move;
  //int queen_character = DFA_WHITE_QUEEN + side_to_move;
  int rook_castle_character = DFA_WHITE_ROOK_CASTLE + side_to_move;
  int rook_character = DFA_WHITE_ROOK + side_to_move;

  // build graph

  Profile profile("build_move_graph");

  // build move graph

  MoveGraph move_graph(get_shape());

  auto connect = [&](std::string from_node, std::string to_node)
  {
    move_graph.add_edge(from_node + " to " + to_node,
			from_node,
			to_node);
  };

  // add nodes

  move_graph.add_node("begin");

  move_graph.add_node("begin+1");
  move_graph.add_edge("pre legal",
		      "begin",
		      "begin+1",
		      get_positions_legal(side_to_move));

  // pick which piece will move and remove it from the board.
  for(auto from_choice : _get_choices_from(side_to_move))
    {
      int from_character = std::get<0>(from_choice);
      int from_square = std::get<1>(from_choice);

      int from_layer = from_square + CHESS_SQUARE_OFFSET;
      change_vector from_changes = _get_changes();
      from_changes[from_layer] = change_type(from_character, DFA_BLANK);
#if CHESS_SQUARE_OFFSET == 2
      if(from_character == king_character)
	{
	  // temporarily clear king index to avoid fanout setting it later.
	  from_changes[side_to_move] = change_type(from_square, 0);
	}
#endif

      std::string from_name = _get_name_from(from_character, from_square);
      move_graph.add_node(from_name,
			  from_changes);
      move_graph.add_edge(from_name,
			  "begin+1",
			  from_name);
    }

  // pick where the piece will move, but do not place it yet.
  for(auto to_choice : _get_choices_to(side_to_move))
    {
      int to_character = std::get<0>(to_choice);
      int to_square = std::get<1>(to_choice);

      change_vector to_changes = _get_changes();
      // main change happens at a following to/prev node.
#if CHESS_SQUARE_OFFSET == 2
      if(to_character == king_character)
	{
	  // set king index now
	  to_changes[side_to_move] = change_type(0, to_square);
	}
#endif

      std::string to_name = _get_name_to(to_character, to_square);
      move_graph.add_node(to_name,
			  to_changes);
      // will add edges later since they involve per-piece logic.
    }

  // basic moves cover most edges between "from" and "to" nodes.
  for(auto basic_choice : _get_choices_basic(side_to_move))
    {
      int from_character = std::get<0>(basic_choice);
      int from_square = std::get<1>(basic_choice);
      int to_character = std::get<2>(basic_choice);
      int to_square = std::get<3>(basic_choice);
      const std::vector<int>& empty_squares = std::get<4>(basic_choice);
      bool capture_only = std::get<5>(basic_choice);

      move_edge_condition_vector basic_conditions;
      for(int empty_square : empty_squares)
	{
	  int empty_layer = empty_square + CHESS_SQUARE_OFFSET;
	  basic_conditions.push_back(DFAUtil::get_fixed(chess_shape,
							empty_layer,
							DFA_BLANK));
	}
      if(capture_only)
	{
	  basic_conditions.push_back(get_positions_can_move_capture(side_to_move, to_square));
	}

      std::string from_name = _get_name_from(from_character, from_square);
      std::string to_name = _get_name_to(to_character, to_square);
      std::string basic_name = ("basic " + from_name + ", " + to_name);
      move_graph.add_edge(basic_name,
			  from_name,
			  to_name,
			  basic_conditions);
    }

  // place the piece, replacing whatever was previously there.
  for(auto to_prev_choice : _get_choices_to_prev(side_to_move))
    {
      int to_character = std::get<0>(to_prev_choice);
      int to_square = std::get<1>(to_prev_choice);
      int prev_character = std::get<2>(to_prev_choice);

      int to_layer = to_square + CHESS_SQUARE_OFFSET;

      change_vector to_prev_changes = _get_changes();
      to_prev_changes[to_layer] = change_type(prev_character, to_character);

      std::string to_name = _get_name_to(to_character, to_square);
      std::string to_prev_name = _get_name_to_prev(to_character, to_square, prev_character);
      move_graph.add_node(to_prev_name,
			  to_prev_changes);
      move_graph.add_edge(to_prev_name,
			  to_name,
			  to_prev_name);
    }

  // en passant moves

  int pawn_rank_start = (side_to_move == SIDE_WHITE) ? 6 : 1;
  int pawn_direction = (side_to_move == SIDE_WHITE) ? -1 : 1;
  int capture_en_passant_character = (side_to_move == SIDE_WHITE) ? DFA_BLACK_PAWN_EN_PASSANT : DFA_WHITE_PAWN_EN_PASSANT;

  std::vector<std::string> en_passant_nodes;
  for(int en_passant_file = 0; en_passant_file < 8; ++en_passant_file)
    {
      change_vector en_passant_changes = _get_changes();

      int to_rank = pawn_rank_start + pawn_direction * 4;
      int to_square = _calc_square(to_rank, en_passant_file);
      int to_layer = _calc_layer(to_rank, en_passant_file);
      en_passant_changes[to_layer] = change_type(DFA_BLANK, pawn_character);

      int capture_rank = pawn_rank_start + pawn_direction * 3;
      int capture_layer = _calc_layer(capture_rank, en_passant_file);
      en_passant_changes[capture_layer] = change_type(capture_en_passant_character, DFA_BLANK);

      std::string en_passant_name = "en_passant to=" + std::to_string(to_square);
      move_graph.add_node(en_passant_name,
			  en_passant_changes);
      en_passant_nodes.push_back(en_passant_name);

      if(en_passant_file > 0)
	{
	  int from_file = en_passant_file - 1;
	  int from_square = _calc_square(capture_rank, from_file);
	  std::string from_name = _get_name_from(pawn_character, from_square);

	  move_graph.add_edge(from_name + ", " + en_passant_name,
			      from_name,
			      en_passant_name);
	}
    }

  // castle left

  int castle_rank = _calc_castle_rank(side_to_move);

  change_vector castle_left_changes = _get_changes();
#if CHESS_SQUARE_OFFSET == 2
  castle_left_changes[side_to_move] = change_type(0, _calc_square(castle_rank, 2));
#endif
  castle_left_changes[_calc_layer(castle_rank, 0)] = change_type(rook_castle_character, DFA_BLANK);
  castle_left_changes[_calc_layer(castle_rank, 1)] = change_type(DFA_BLANK, DFA_BLANK);
  castle_left_changes[_calc_layer(castle_rank, 2)] = change_type(DFA_BLANK, king_character);
  castle_left_changes[_calc_layer(castle_rank, 3)] = change_type(DFA_BLANK, rook_character);
  castle_left_changes[_calc_layer(castle_rank, 4)] = change_type(DFA_BLANK, DFA_BLANK); // king's old position

  move_edge_condition_vector castle_left_conditions;
  for(int threat_file = 2; threat_file <= 4; ++threat_file)
    {
      shared_dfa_ptr threat = get_positions_threat(side_to_move, _calc_square(castle_rank, threat_file));
      shared_dfa_ptr no_threat = DFAUtil::get_inverse(threat);
      castle_left_conditions.push_back(no_threat);
    }

  move_graph.add_node("castle left",
		      castle_left_changes);
  move_graph.add_edge("castle left",
		      _get_name_from(king_character, _calc_square(castle_rank, 4)),
		      "castle left",
		      castle_left_conditions);

  // castle right

  change_vector castle_right_changes = _get_changes();
#if CHESS_SQUARE_OFFSET == 2
  castle_right_changes[side_to_move] = change_type(0, _calc_square(castle_rank, 6));
#endif
  castle_right_changes[_calc_layer(castle_rank, 4)] = change_type(DFA_BLANK, DFA_BLANK); // king's old position
  castle_right_changes[_calc_layer(castle_rank, 5)] = change_type(DFA_BLANK, rook_character);
  castle_right_changes[_calc_layer(castle_rank, 6)] = change_type(DFA_BLANK, king_character);
  castle_right_changes[_calc_layer(castle_rank, 7)] = change_type(rook_castle_character, DFA_BLANK);

  move_edge_condition_vector castle_right_conditions;
  for(int threat_file = 4; threat_file <= 6; ++threat_file)
    {
      shared_dfa_ptr threat = get_positions_threat(side_to_move, _calc_square(castle_rank, threat_file));
      shared_dfa_ptr no_threat = DFAUtil::get_inverse(threat);
      castle_right_conditions.push_back(no_threat);
    }

  move_graph.add_node("castle right",
		      castle_right_changes);
  move_graph.add_edge("castle right",
		      _get_name_from(king_character, _calc_square(castle_rank, 4)),
		      "castle right",
		      castle_right_conditions);

  // clear castle rights

  move_graph.add_node("clear castle begin");

  change_vector clear_castle_both_changes = _get_changes();
  clear_castle_both_changes[_calc_layer(castle_rank, 0)] = change_type(rook_castle_character, rook_character);
  clear_castle_both_changes[_calc_layer(castle_rank, 7)] = change_type(rook_castle_character, rook_character);
  move_graph.add_node("clear castle both",
		      clear_castle_both_changes);
  connect("clear castle begin", "clear castle both");

  change_vector clear_castle_left_changes = _get_changes();
  clear_castle_left_changes[_calc_layer(castle_rank, 0)] = change_type(rook_castle_character, rook_character);
  move_graph.add_node("clear castle left",
		      clear_castle_left_changes);
  connect("castle right", "clear castle left");
  connect("clear castle begin", "clear castle left");

  move_graph.add_node("clear castle none");
  connect("castle left", "clear castle none");
  connect("castle right", "clear castle none");
  connect("clear castle begin", "clear castle none");

  change_vector clear_castle_right_changes = _get_changes();
  clear_castle_right_changes[_calc_layer(castle_rank, 7)] = change_type(rook_castle_character, rook_character);
  move_graph.add_node("clear castle right",
		      clear_castle_right_changes);
  connect("castle left", "clear castle right");
  connect("clear castle begin", "clear castle right");

  shared_dfa_ptr no_castle_rights = DFAUtil::get_count_character(get_shape(), rook_castle_character, 0, 0, CHESS_SQUARE_OFFSET);
  move_edge_condition_vector clear_castle_end_conditions({no_castle_rights});

  move_graph.add_node("clear castle end",
		      _get_changes(),
		      clear_castle_end_conditions,
		      clear_castle_end_conditions);
  connect("clear castle both", "clear castle end");
  connect("clear castle left", "clear castle end");
  connect("clear castle none", "clear castle end");
  connect("clear castle right", "clear castle end");

  // clear en passant status

  move_graph.add_node("clear en passant begin");
  connect("clear castle end", "clear en passant begin");

  move_graph.add_node("clear en passant none");

  std::vector<std::string> clear_en_passant_names;
  clear_en_passant_names.push_back("clear en passant none");

  int clear_en_passant_before_character = DFA_WHITE_PAWN_EN_PASSANT + (1 - side_to_move);
  int clear_en_passant_after_character = DFA_WHITE_PAWN + (1 - side_to_move);
  int clear_en_passant_rank = pawn_rank_start + pawn_direction * 3;
  for(int clear_en_passant_file = 0;
      clear_en_passant_file < 8;
      ++clear_en_passant_file)
    {
      int clear_en_passant_square = _calc_square(clear_en_passant_rank, clear_en_passant_file);
      int clear_en_passant_layer = _calc_layer(clear_en_passant_rank, clear_en_passant_file);

      change_vector clear_en_passant_changes = _get_changes();
      clear_en_passant_changes[clear_en_passant_layer] = change_type(clear_en_passant_before_character, clear_en_passant_after_character);

      std::string clear_en_passant_name = "clear en passant square=" + std::to_string(clear_en_passant_square);
      move_graph.add_node(clear_en_passant_name, clear_en_passant_changes);
      clear_en_passant_names.push_back(clear_en_passant_name);
    }

  shared_dfa_ptr cleared_en_passant_condition = DFAUtil::get_count_character(get_shape(),
									     clear_en_passant_before_character,
									     0, 0,
									     CHESS_SQUARE_OFFSET);
  move_graph.add_node("clear en passant end",
		      _get_changes(),
		      move_edge_condition_vector({cleared_en_passant_condition}),
		      move_edge_condition_vector({cleared_en_passant_condition}));

  for(std::string clear_en_passant_name : clear_en_passant_names)
    {
      connect("clear en passant begin", clear_en_passant_name);
      connect(clear_en_passant_name, "clear en passant end");
    }

  // connect to/prev nodes to clear castle or clear en passant nodes

  for(auto to_prev_choice : _get_choices_to_prev(side_to_move))
    {
      int to_character = std::get<0>(to_prev_choice);
      int to_square = std::get<1>(to_prev_choice);
      int prev_character = std::get<2>(to_prev_choice);

      std::string to_prev_name = _get_name_to_prev(to_character, to_square, prev_character);

      // TODO : only clear castle rights after first king move
      std::string next_name = ((to_character == king_character) ?
			       "clear castle begin" :
			       "clear en passant begin");

      connect(to_prev_name, next_name);
    }

  // connect en_passant nodes to clear en passant end since the
  // capture clears the en passant status.

  for(std::string en_passant_node : en_passant_nodes)
    {
      connect(en_passant_node, "clear en passant end");
    }

  // apply post constraints at end

  move_graph.add_node("end-2");
  connect("clear en passant end", "end-2");

  move_graph.add_node("end-1");
  move_graph.add_edge("post not check",
		      "end-2",
		      "end-1",
		      get_positions_not_check(side_to_move));

  move_graph.add_node("end");
  move_graph.add_edge("post legal",
		      "end-1",
		      "end",
		      get_positions_legal(1 - side_to_move));

  // done

  return move_graph;
}

shared_dfa_ptr ChessGame::from_board(const Board& board_in)
{
  std::vector<DFAString> dfa_strings;
  dfa_strings.push_back(from_board_to_dfa_string(board_in));

  return DFAUtil::from_strings(chess_shape, dfa_strings);
}

DFAString ChessGame::from_board_to_dfa_string(const Board& board_in)
{
  const auto& pieces_by_side = board_in.pieces_by_side;
  const auto& pieces_by_side_type = board_in.pieces_by_side_type;

  std::vector<int> board_characters;

#if CHESS_SQUARE_OFFSET == 2
  // white king index
  assert(pieces_by_side_type[SIDE_WHITE][PIECE_KING]);
  board_characters.push_back(std::countr_zero(pieces_by_side_type[SIDE_WHITE][PIECE_KING]));
  // black king index
  assert(pieces_by_side_type[SIDE_BLACK][PIECE_KING]);
  board_characters.push_back(std::countr_zero(pieces_by_side_type[SIDE_BLACK][PIECE_KING]));
#endif

  for(int square = 0; square < 64; ++square)
    {
      int square_rank = square / 8;
      int square_file = square % 8;

      BoardMask square_mask = 1ULL << square;
      std::function<bool(int,int)> check = [=](int s, int p) {
	return (pieces_by_side_type[s][p] & square_mask) != 0;
      };
      if(pieces_by_side[SIDE_WHITE] & square_mask)
	{
	  if(check(SIDE_WHITE, PIECE_KING))
	    {
	      board_characters.push_back(DFA_WHITE_KING);
	    }
	  else if(check(SIDE_WHITE, PIECE_QUEEN))
	    {
	      board_characters.push_back(DFA_WHITE_QUEEN);
	    }
	  else if(check(SIDE_WHITE, PIECE_BISHOP))
	    {
	      board_characters.push_back(DFA_WHITE_BISHOP);
	    }
	  else if(check(SIDE_WHITE, PIECE_KNIGHT))
	    {
	      board_characters.push_back(DFA_WHITE_KNIGHT);
	    }
	  else if(check(SIDE_WHITE, PIECE_ROOK))
	    {
	      if(board_in.castling_availability & square_mask)
		{
		  board_characters.push_back(DFA_WHITE_ROOK_CASTLE);
		}
	      else
		{
		  board_characters.push_back(DFA_WHITE_ROOK);
		}
	    }
	  else if(check(SIDE_WHITE, PIECE_PAWN))
	    {
	      if((board_in.side_to_move == SIDE_BLACK) && (square_rank == 4) && (square_file == board_in.en_passant_file))
		{
		  board_characters.push_back(DFA_WHITE_PAWN_EN_PASSANT);
		}
	      else
		{
		  board_characters.push_back(DFA_WHITE_PAWN);
		}
	    }
	  else
	    {
	      // invalid board
	      assert(false);
	    }
	}
      else if(pieces_by_side[SIDE_BLACK] & square_mask)
	{
	  if(check(SIDE_BLACK, PIECE_KING))
	    {
	      board_characters.push_back(DFA_BLACK_KING);
	    }
	  else if(check(SIDE_BLACK, PIECE_QUEEN))
	    {
	      board_characters.push_back(DFA_BLACK_QUEEN);
	    }
	  else if(check(SIDE_BLACK, PIECE_BISHOP))
	    {
	      board_characters.push_back(DFA_BLACK_BISHOP);
	    }
	  else if(check(SIDE_BLACK, PIECE_KNIGHT))
	    {
	      board_characters.push_back(DFA_BLACK_KNIGHT);
	    }
	  else if(check(SIDE_BLACK, PIECE_ROOK))
	    {
	      if(board_in.castling_availability & square_mask)
		{
		  board_characters.push_back(DFA_BLACK_ROOK_CASTLE);
		}
	      else
		{
		  board_characters.push_back(DFA_BLACK_ROOK);
		}
	    }
	  else if(check(SIDE_BLACK, PIECE_PAWN))
	    {
	      if((board_in.side_to_move == SIDE_WHITE) && (square_rank == 3) && (square_file == board_in.en_passant_file))
		{
		  board_characters.push_back(DFA_BLACK_PAWN_EN_PASSANT);
		}
	      else
		{
		  board_characters.push_back(DFA_BLACK_PAWN);
		}
	    }
	  else
	    {
	      // invalid board
	      assert(false);
	    }
	}
      else
	{
	  board_characters.push_back(DFA_BLANK);
	}
    }

  assert(board_characters.size() == 64 + CHESS_SQUARE_OFFSET);

  return DFAString(chess_shape, board_characters);
}

shared_dfa_ptr ChessGame::get_positions_can_move_basic(int side_to_move, int square) const
{
  shared_dfa_ptr singletons[2][64] = {{0}};

  if(!singletons[side_to_move][square])
    {
      singletons[side_to_move][square] =
        load_or_build("can_move_basic-side=" + std::to_string(side_to_move) + ",square=" + std::to_string(square),
		      [&]()
		      {
			shared_dfa_ptr empty = DFAUtil::get_fixed(get_shape(), square + CHESS_SQUARE_OFFSET, DFA_BLANK);
			return DFAUtil::get_union(empty,
						  get_positions_can_move_capture(side_to_move, square));
		      });
    }

  return singletons[side_to_move][square];
}

shared_dfa_ptr ChessGame::get_positions_can_move_capture(int side_to_move, int square) const
{
  shared_dfa_ptr singletons[2][64] = {{0}};

  if(!singletons[side_to_move][square])
    {
      singletons[side_to_move][square] =
	load_or_build("can_move_capture-side=" + std::to_string(side_to_move) + ",square=" + std::to_string(square),
		      [&]()
		      {
			int layer = square + CHESS_SQUARE_OFFSET;

			std::vector<shared_dfa_ptr> partials;
			for(int c = 0; c < chess_shape[layer]; ++c)
			  {
			    if(chess_is_hostile(side_to_move, c) && (c != DFA_BLACK_KING) && (c != DFA_WHITE_KING))
			      {
				partials.push_back(DFAUtil::get_fixed(get_shape(), layer, c));
			      }
			  }

			return DFAUtil::get_union_vector(chess_shape, partials);
		      });
    }

  return singletons[side_to_move][square];
}

shared_dfa_ptr ChessGame::get_positions_check(int checked_side) const
{
  Profile profile("get_positions_check");

  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[checked_side])
    {
      singletons[checked_side] = load_or_build("check_positions-side=" + std::to_string(checked_side), [&]()
      {
	profile.tic("singleton");

	// constrain number of kings to one. otherwise threat checks blow up a lot.
	int king_character = (checked_side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;
	shared_dfa_ptr king_positions = get_positions_king(checked_side);

	std::vector<shared_dfa_ptr> checks;
	for(int square = 0; square < 64; ++square)
	  {
	    std::vector<shared_dfa_ptr> square_requirements;
	    square_requirements.emplace_back(DFAUtil::get_fixed(chess_shape, square + CHESS_SQUARE_OFFSET, king_character));
	    square_requirements.push_back(king_positions); // makes union much cheaper below
	    square_requirements.push_back(get_positions_threat(checked_side, square));

	    checks.emplace_back(DFAUtil::get_intersection_vector(chess_shape, square_requirements));
	  }

	return DFAUtil::get_union_vector(chess_shape, checks);
      });
    }

  return singletons[checked_side];
}

shared_dfa_ptr ChessGame::get_positions_king(int king_side) const
{
  Profile profile("get_positions_king");

  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[king_side])
    {
      singletons[king_side] = load_or_build("king_positions-side=" + std::to_string(king_side), [=]()
      {
	int king_character = (king_side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

	shared_dfa_ptr count_constraint(new CountCharacterDFA(chess_shape, king_character, 1, 1, CHESS_SQUARE_OFFSET));

#if CHESS_SQUARE_OFFSET == 0
	return count_constraint;
#elif CHESS_SQUARE_OFFSET == 2
	std::vector<shared_dfa_ptr> king_choices;
	for(int king_square = 0; king_square < 64; ++king_square)
	  {
	    std::vector<shared_dfa_ptr> king_square_requirements;

	    // king index
	    king_square_requirements.emplace_back(DFAUtil::get_fixed(chess_shape, king_side, king_square));
	    // king on target square
	    king_square_requirements.emplace_back(DFAUtil::get_fixed(chess_shape, CHESS_SQUARE_OFFSET + king_square, king_character));
	    // exactly one king
	    king_square_requirements.emplace_back(count_constraint);

	    king_choices.emplace_back(DFAUtil::get_intersection_vector(chess_shape, king_square_requirements));
	    assert(king_choices[king_square]->size() > 0);
	  }

	return DFAUtil::get_union_vector(chess_shape, king_choices);
#else
#error
#endif
      });
    }
  return singletons[king_side];
}

shared_dfa_ptr ChessGame::get_positions_legal(int side_to_move) const
{
  Profile profile("get_positions_legal");

  // legal position requirements:
  // * make sure initial king indexes are correct
  // * pawns in appropriate rows
  // * current player has no en-passant pawns
  // * opposing player not in check

  return load_or_build("legal_positions,side_to_move=" + std::to_string(side_to_move), [&]()
  {
    profile.tic("requirements");

    std::vector<shared_dfa_ptr> requirements;
    std::function<void(shared_dfa_ptr)> add_requirement = [&](shared_dfa_ptr requirement)
    {
      assert(requirement->states() > 0);
      assert(requirement->ready());
      requirements.emplace_back(requirement);
    };

    add_requirement(get_positions_legal_shared());

    // en-passant pawn restrictions

    add_requirement(DFAUtil::get_count_character(chess_shape,
						 (side_to_move == SIDE_BLACK) ? DFA_BLACK_PAWN_EN_PASSANT : DFA_WHITE_PAWN_EN_PASSANT,
						 0, 0,
						 CHESS_SQUARE_OFFSET));

    std::cout << "get_positions_legal() => " << requirements.size() << " requirements" << std::endl;

    profile.tic("intersection");

    return DFAUtil::get_intersection_vector(chess_shape, requirements);
  });
}

shared_dfa_ptr ChessGame::get_positions_legal_shared() const
{
  Profile profile("get_positions_legal_shared");

  // legal position requirements:
  // * make sure initial king indexes are correct
  // * pawns in appropriate rows

  return load_or_build("legal_positions_shared", [&]()
  {
    profile.tic("requirements");

    std::vector<shared_dfa_ptr> requirements;
    std::function<void(shared_dfa_ptr)> add_requirement = [&](shared_dfa_ptr requirement)
    {
      assert(requirement->states() > 0);
      assert(requirement->ready());
      requirements.emplace_back(requirement);
    };

    std::function<void(int, int)> block_file = [&](int file, int character)
    {
      for(int rank = 0; rank < 8; ++rank)
	{
	  int square = rank * 8 + file;

	  add_requirement(DFAUtil::get_inverse(DFAUtil::get_fixed(chess_shape, square + CHESS_SQUARE_OFFSET, character)));
	}
    };

    std::function<void(int, int)> block_row = [&](int rank, int character)
    {
      for(int file = 0; file < 8; ++file)
	{
	  int square = rank * 8 + file;

	  add_requirement(DFAUtil::get_inverse(DFAUtil::get_fixed(chess_shape, square + CHESS_SQUARE_OFFSET, character)));
	}
    };

    // pawn restrictions

    block_row(0, DFA_BLACK_PAWN);
    block_row(7, DFA_BLACK_PAWN);
    block_row(0, DFA_WHITE_PAWN);
    block_row(7, DFA_WHITE_PAWN);

    // en-passant pawn restrictions

    block_row(0, DFA_BLACK_PAWN_EN_PASSANT);
    block_row(1, DFA_BLACK_PAWN_EN_PASSANT);
    block_row(2, DFA_BLACK_PAWN_EN_PASSANT);
    block_row(4, DFA_BLACK_PAWN_EN_PASSANT);
    block_row(5, DFA_BLACK_PAWN_EN_PASSANT);
    block_row(6, DFA_BLACK_PAWN_EN_PASSANT);
    block_row(7, DFA_BLACK_PAWN_EN_PASSANT);

    block_row(0, DFA_WHITE_PAWN_EN_PASSANT);
    block_row(1, DFA_WHITE_PAWN_EN_PASSANT);
    block_row(2, DFA_WHITE_PAWN_EN_PASSANT);
    block_row(3, DFA_WHITE_PAWN_EN_PASSANT);
    block_row(5, DFA_WHITE_PAWN_EN_PASSANT);
    block_row(6, DFA_WHITE_PAWN_EN_PASSANT);
    block_row(7, DFA_WHITE_PAWN_EN_PASSANT);

    // castle/rook restrictions

    for(int file = 1; file < 7; ++file)
      {
	block_file(file, DFA_BLACK_ROOK_CASTLE);
	block_file(file, DFA_WHITE_ROOK_CASTLE);
      }

    for(int rank = 1; rank < 8; ++rank)
      {
	block_row(rank, DFA_BLACK_ROOK_CASTLE);
      }

    for(int rank = 0; rank < 7; ++rank)
      {
	block_row(rank, DFA_WHITE_ROOK_CASTLE);
      }

    // king restrictions
    requirements.push_back(get_positions_king(0));
    requirements.push_back(get_positions_king(1));

    // castle rights

    for(int side = 0; side < 2; ++side)
      {
	int king_character = (side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;
	int king_rank = _calc_castle_rank(side);
	int king_square = king_rank * 8 + 4;

	int castle_rook_character = (side == SIDE_WHITE) ? DFA_WHITE_ROOK_CASTLE : DFA_BLACK_ROOK_CASTLE;

	shared_dfa_ptr king_initial_position = DFAUtil::get_fixed(chess_shape, king_square + CHESS_SQUARE_OFFSET, king_character);
	shared_dfa_ptr castle_rights_cleared = DFAUtil::get_count_character(chess_shape, castle_rook_character, 0, 0, CHESS_SQUARE_OFFSET);

	requirements.push_back(DFAUtil::get_union(king_initial_position, castle_rights_cleared));
      }

    // ready to combine

    std::cout << "get_positions_legal_shared() => " << requirements.size() << " requirements" << std::endl;

    profile.tic("intersection");

    return DFAUtil::get_intersection_vector(chess_shape, requirements);
  });
}

shared_dfa_ptr ChessGame::get_positions_not_check(int checked_side) const
{
  // opposing side is not in check

  return load_or_build("not_check_positions-side=" + std::to_string(checked_side), [&]()
  {
    shared_dfa_ptr check_positions = get_positions_check(checked_side);
    return DFAUtil::get_inverse(check_positions);
  });
}

shared_dfa_ptr ChessGame::get_positions_threat(int threatened_side, int threatened_square) const
{
  Profile profile("get_positions_threat");

  static shared_dfa_ptr singletons[2][64];

  if(!singletons[threatened_side][threatened_square])
    {
      singletons[threatened_side][threatened_square] = \
	load_or_build("threat_positions-side=" + std::to_string(threatened_side) + ",square=" + std::to_string(threatened_square), [=]()
	{
	  BoardMask threatened_mask = 1ULL << threatened_square;
	  std::vector<shared_dfa_ptr> threats;
	  std::function<void(int, const MoveSet&)> add_threat = [&](int threatening_character, const MoveSet& moves)
	  {
	    for(int threatening_square = 0; threatening_square < 64; ++threatening_square)
	      {
		if(!(moves.moves[threatening_square] & threatened_mask))
		  {
		    continue;
		  }

		std::vector<shared_dfa_ptr> threat_components;
		threat_components.emplace_back(DFAUtil::get_fixed(chess_shape, threatening_square + CHESS_SQUARE_OFFSET, threatening_character));

		BoardMask between_mask = between_masks.masks[threatening_square][threatened_square];
		for(int between_square = std::countr_zero(between_mask); between_square < 64; between_square += 1 + std::countr_zero(between_mask >> (between_square + 1)))
		  {
		    threat_components.emplace_back(DFAUtil::get_fixed(chess_shape, between_square + CHESS_SQUARE_OFFSET, DFA_BLANK));
		  }

		threats.push_back(DFAUtil::get_intersection_vector(chess_shape, threat_components));
	      }
	  };

	  if(threatened_side == SIDE_WHITE)
	    {
	      add_threat(DFA_BLACK_BISHOP, bishop_moves);
	      add_threat(DFA_BLACK_KING, king_moves);
	      add_threat(DFA_BLACK_KNIGHT, knight_moves);
	      add_threat(DFA_BLACK_PAWN, pawn_captures_black);
	      add_threat(DFA_BLACK_PAWN_EN_PASSANT, pawn_captures_black);
	      add_threat(DFA_BLACK_QUEEN, queen_moves);
	      add_threat(DFA_BLACK_ROOK, rook_moves);
	      add_threat(DFA_BLACK_ROOK_CASTLE, rook_moves);
	    }
	  else
	    {
	      add_threat(DFA_WHITE_BISHOP, bishop_moves);
	      add_threat(DFA_WHITE_KING, king_moves);
	      add_threat(DFA_WHITE_KNIGHT, knight_moves);
	      add_threat(DFA_WHITE_PAWN, pawn_captures_white);
	      add_threat(DFA_WHITE_PAWN_EN_PASSANT, pawn_captures_white);
	      add_threat(DFA_WHITE_QUEEN, queen_moves);
	      add_threat(DFA_WHITE_ROOK, rook_moves);
	      add_threat(DFA_WHITE_ROOK_CASTLE, rook_moves);
	    }

	  return DFAUtil::get_union_vector(chess_shape, threats);
	});
    }

  return singletons[threatened_side][threatened_square];
}

DFAString ChessGame::get_position_initial() const
{
  Profile profile("get_positions_initial");

  Board initial_board(INITIAL_FEN);
  return from_board_to_dfa_string(initial_board);
}

shared_dfa_ptr ChessGame::get_positions_lost(int side_to_move) const
{
  Profile profile("get_positions_lost");

  // lost if and only if check and no legal moves

  return load_or_build(get_name_lost(side_to_move),
		       [&]()
		       {
			 return DFAUtil::get_difference(get_positions_check(side_to_move),
							get_has_moves(side_to_move));
		       });
}

shared_dfa_ptr ChessGame::get_positions_won(int side_to_move) const
{
  Profile profile("get_positions_won");

  // lost if and only if check and no legal moves

  return DFAUtil::get_reject(chess_shape);
}

Board ChessGame::position_to_board(int side_to_move, const DFAString& position_in) const
{
  // will assemble sloppy fen string and pass to Board constructor
  std::string fen_temp("");
  std::string fen_castle("");
  std::string fen_en_passant("");

  for(int square = 0; square < 64; ++square)
    {
      int square_rank = (square / 8) + 1;
      char square_file = 'a' + (square % 8);

      int c = position_in[square + CHESS_SQUARE_OFFSET];
      switch(c)
	{
	case DFA_BLANK:
	  fen_temp += "1";
	  break;

	case DFA_WHITE_KING:
	  fen_temp += "K";
	  break;

	case DFA_BLACK_KING:
	  fen_temp += "k";
	  break;

	case DFA_WHITE_QUEEN:
	  fen_temp += "Q";
	  break;

	case DFA_BLACK_QUEEN:
	  fen_temp += "q";
	  break;

	case DFA_WHITE_BISHOP:
	  fen_temp += "B";
	  break;

	case DFA_BLACK_BISHOP:
	  fen_temp += "b";
	  break;

	case DFA_WHITE_KNIGHT:
	  fen_temp += "N";
	  break;

	case DFA_BLACK_KNIGHT:
	  fen_temp += "n";
	  break;

	case DFA_WHITE_ROOK:
	  fen_temp += "R";
	  break;

	case DFA_BLACK_ROOK:
	  fen_temp += "r";
	  break;

	case DFA_WHITE_PAWN:
	  fen_temp += "P";
	  break;

	case DFA_BLACK_PAWN:
	  fen_temp += "p";
	  break;

	case DFA_WHITE_PAWN_EN_PASSANT:
	  fen_temp += "P";
	  fen_en_passant = "";
	  fen_en_passant += square_file;
	  fen_en_passant += ('1' + (square_rank - 1));
	  break;

	case DFA_BLACK_PAWN_EN_PASSANT:
	  fen_temp += "p";
	  fen_en_passant = "";
	  fen_en_passant += square_file;
	  fen_en_passant += ('1' + (square_rank + 1));
	  break;

	case DFA_WHITE_ROOK_CASTLE:
	  fen_temp += "R";
	  if(square % 8 == 0)
	    {
	      // queen side
	      fen_castle = "Q" + fen_castle;
	    }
	  else
	    {
	      // king side
	      fen_castle = "K" + fen_castle;
	    }
	  break;

	case DFA_BLACK_ROOK_CASTLE:
	  fen_temp += "r";
	  if(square % 8 == 0)
	    {
	      // queen side
	      fen_castle = "q" + fen_castle;
	    }
	  else
	    {
	      // king side
	      fen_castle = "k" + fen_castle;
	    }
	  break;

	default:
	  assert(0);
	}

      if((square % 8 == 7) && (square < 63))
	{
	  fen_temp += "/";
	}
    }
  assert(fen_temp.length() == 64 + 7);

  if(side_to_move == SIDE_WHITE)
    {
      fen_temp += " w";
    }
  else
    {
      fen_temp += " b";
    }

  if(fen_castle.length() > 0)
    {
      fen_temp += " " + fen_castle;
    }
  else
    {
      // no castling left
      fen_temp += " -";
    }

  if(fen_en_passant.length() > 0)
    {
      fen_temp += " " + fen_en_passant;
    }
  else
    {
      fen_temp += " -";
    }

  // don't care about move counts
  fen_temp += " 0 1";

  return Board(fen_temp);
}

std::string ChessGame::position_to_string(const DFAString& position_in) const
{
  // side to move does not matter with current string output
  return position_to_board(SIDE_WHITE, position_in).to_string();
}
