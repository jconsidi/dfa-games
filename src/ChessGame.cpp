// ChessGame.cpp

#include "ChessGame.h"

#include <bit>
#include <functional>
#include <iostream>
#include <string>

#include "BetweenMasks.h"
#include "CountCharacterDFA.h"
#include "DFAUtil.h"
#include "MoveSet.h"
#include "Profile.h"

static const dfa_shape_t chess_shape = {CHESS_DFA_SHAPE};

static void add_between_condition(int from_square,
				  int to_square,
				  std::vector<shared_dfa_ptr>& conditions)
{
  // require squares between from and to squares to be empty pre and post.
  BoardMask between_mask = between_masks.masks[from_square][to_square];

  auto check_square =
    [&](int square)
    {
      if(between_mask & (1ULL << square))
	{
	  int layer = square + CHESS_SQUARE_OFFSET;
	  shared_dfa_ptr fixed_blank = DFAUtil::get_fixed(chess_shape, layer, DFA_BLANK);
	  conditions.push_back(fixed_blank);
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
}

static void add_change(int square,
		       int pre_character,
		       int post_character,
		       std::vector<shared_dfa_ptr>& pre_conditions,
		       change_vector& changes,
		       std::vector<shared_dfa_ptr>& post_conditions)
{
  int layer = square + CHESS_SQUARE_OFFSET;

  // clear rook's castle rights. done here so that the post condition
  // doesn't try to keep them and conflict.
  if(post_character == DFA_WHITE_ROOK_CASTLE)
    {
      post_character = DFA_WHITE_ROOK;
    }
  else if(post_character == DFA_BLACK_ROOK_CASTLE)
    {
      post_character = DFA_BLACK_ROOK;
    }

  pre_conditions.push_back(DFAUtil::get_fixed(chess_shape, layer, pre_character));

  assert(!changes[layer].has_value());
  changes[layer] = change_type(pre_character, post_character);

  post_conditions.push_back(DFAUtil::get_fixed(chess_shape, layer, post_character));
}

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

static std::string get_to_node_name(int to_character, int to_square)
{
  return "to_char=" + std::to_string(to_character) + ", to_square=" + std::to_string(to_square);
}

ChessGame::ChessGame()
  : Game("chess+" + std::to_string(CHESS_SQUARE_OFFSET), chess_shape)
{
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

shared_dfa_ptr ChessGame::get_positions_legal() const
{
  Profile profile("get_positions_legal");

  // basic position requirements:
  // * make sure initial king indexes are correct
  // * pawns in appropriate rows

  static shared_dfa_ptr singleton;
  if(!singleton)
    {
      singleton = load_or_build("legal_positions", [=]()
      {
	Profile profile("ChessGame::get_positions_legal()");
	profile.tic("singleton");

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

	std::cout << "get_positions_legal() => " << requirements.size() << " requirements" << std::endl;

	return DFAUtil::get_intersection_vector(chess_shape, requirements);
      });
    }

  return singleton;
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
      singletons[checked_side] = load_or_build("check_positions-side=" + std::to_string(checked_side), [=]()
      {
	Profile profile(std::ostringstream() << "ChessGame::get_positions_check(" << checked_side << ")");
	profile.tic("singleton");

	shared_dfa_ptr basic_positions = get_positions_legal();
	int king_character = (checked_side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

	std::vector<shared_dfa_ptr> checks;
	for(int square = 0; square < 64; ++square)
	  {
	    std::vector<shared_dfa_ptr> square_requirements;
	    square_requirements.emplace_back(DFAUtil::get_fixed(chess_shape, square + CHESS_SQUARE_OFFSET, king_character));
	    square_requirements.push_back(basic_positions); // makes union much cheaper below
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

  return DFAUtil::get_difference(get_positions_check(side_to_move),
						   this->get_has_moves(side_to_move));
}

shared_dfa_ptr ChessGame::get_positions_won(int side_to_move) const
{
  Profile profile("get_positions_won");

  // lost if and only if check and no legal moves

  return DFAUtil::get_reject(chess_shape);
}

MoveGraph ChessGame::build_move_graph(int side_to_move) const
{
  Profile profile("get_phases_internal");

  shared_dfa_ptr basic_positions = get_positions_legal();
  shared_dfa_ptr check_positions = get_positions_check(side_to_move);
  shared_dfa_ptr not_check_positions = load_or_build("not_check_positions-side=" + std::to_string(side_to_move), [=]()
  {
    return DFAUtil::get_inverse(check_positions);
  });

  // shared pre/post conditions for all moves
  shared_dfa_ptr pre_shared = basic_positions;
  shared_dfa_ptr post_shared = load_or_build("post_shared-side=" + std::to_string(side_to_move), [=]()
  {
    return DFAUtil::get_intersection(basic_positions, not_check_positions);
  });

  // collect choices

  MoveGraph move_graph;
  move_graph.add_node("begin");
  move_graph.add_node("pre shared");

  int to_character_min = (side_to_move == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;
  int to_character_max = (side_to_move == SIDE_WHITE) ? DFA_WHITE_PAWN : DFA_BLACK_PAWN;
  std::vector<std::pair<int, int>> to_choices;
  for(int to_character = to_character_min; to_character <= to_character_max; to_character += 2)
    {
      int to_character_pawn = (to_character == DFA_WHITE_PAWN) || (to_character == DFA_BLACK_PAWN);
      int to_square_min = (!to_character_pawn) ? 0 : 8;
      int to_square_max = (!to_character_pawn) ? 63 : 57;

      for(int to_square = to_square_min; to_square <= to_square_max; ++to_square)
	{
	  to_choices.emplace_back(to_character, to_square);
	  move_graph.add_node(get_to_node_name(to_character, to_square));
	}
    }

  move_graph.add_node("clear castle rights");
  move_graph.add_node("clear en passant");
  move_graph.add_node("post shared");
  move_graph.add_node("end");

  move_graph.add_edge("pre shared",
		      "begin",
		      "pre shared",
		      move_edge_condition_vector({pre_shared}),
		      change_vector(64 + CHESS_SQUARE_OFFSET),
		      move_edge_condition_vector({}));

  std::function<void(int, const MoveSet&)> add_basic_choices =
    [&](int character, const MoveSet& moves)
    {
      for(int from_square = 0; from_square < 64; ++from_square)
	{
	  int from_layer = from_square + CHESS_SQUARE_OFFSET;
	  int from_layer_shape = basic_positions->get_layer_shape(from_layer);
	  if(character >= from_layer_shape)
	    {
	      continue;
	    }

	  if((character == DFA_BLACK_ROOK_CASTLE) &&
	     (from_square != 0) &&
	     (from_square != 7))
	    {
	      // black rook with castle privileges can only be in two positions.
	      continue;
	    }

	  if((character == DFA_WHITE_ROOK_CASTLE) &&
	     (from_square != 56) &&
	     (from_square != 63))
	    {
	      // white rook with castle privileges can only be in two positions.
	      continue;
	    }

	  int to_character = character;
	  if(character == DFA_BLACK_ROOK_CASTLE)
	    {
	      to_character = DFA_BLACK_ROOK;
	    }
	  if(character == DFA_WHITE_ROOK_CASTLE)
	    {
	      to_character = DFA_WHITE_ROOK;
	    }

	  for(int to_square = 0; to_square < 64; ++to_square)
	    {
	      if(!(moves.moves[from_square] & (1ULL << to_square)))
		{
		  continue;
		}
	      assert(to_square != from_square);

	      std::vector<shared_dfa_ptr> basic_pre_conditions = {};
	      change_vector basic_changes(64 + CHESS_SQUARE_OFFSET);
	      std::vector<shared_dfa_ptr> basic_post_conditions = {};

	      // from conditions - will be shared across simple move and captures
	      add_change(from_square,
			 character,
			 DFA_BLANK,
			 basic_pre_conditions,
			 basic_changes,
			 basic_post_conditions);

	      // require squares between from and to squares to be empty pre and post.
	      add_between_condition(from_square,
				    to_square,
				    basic_pre_conditions);
	      add_between_condition(from_square,
				    to_square,
				    basic_post_conditions);

	      // to condition - used to shrink DFA before current change and following "to" node's union...
	      basic_pre_conditions.push_back(get_positions_can_move_basic(side_to_move, to_square)); // forward speedup
	      basic_post_conditions.push_back(get_positions_can_move_basic(side_to_move, to_square)); // NOP for clarity

#if CHESS_SQUARE_OFFSET == 2
	      if(character == DFA_WHITE_KING)
		{
		  // white king index
		  basic_changes[SIDE_WHITE] = change_type(from_square, to_square);
		}

	      if(character == DFA_BLACK_KING)
		{
		  // black king index
		  basic_changes[SIDE_BLACK] = change_type(from_square, to_square);
		}
#endif

	      // moving this piece

	      move_graph.add_edge("basic from_char=" + std::to_string(character) + ", from_square=" + std::to_string(from_square) + ", to_square=" + std::to_string(to_square),
				  "pre shared",
				  get_to_node_name(to_character, to_square),
				  basic_pre_conditions,
				  basic_changes,
				  basic_post_conditions);
	    }
	}
    };

  std::function<void(int, int, int, int, int)> add_pawn_choices =
    [&](int pawn_character, int en_passant_character, int capture_en_passant_character, int rank_start, int rank_direction)
    {
      for(int from_file = 0; from_file < 8; ++from_file)
	{
	  for(int previous_advancement = 0; previous_advancement < 6; ++previous_advancement)
	    {
	      int from_rank = rank_start + previous_advancement * rank_direction;
	      int from_square = from_rank * 8 + from_file;

	      int advance_rank = from_rank + rank_direction;
	      int advance_square = advance_rank * 8 + from_file;
	      int advance_layer = advance_square + CHESS_SQUARE_OFFSET;

	      std::vector<int> advance_characters;
	      if(previous_advancement < 5)
		{
		  advance_characters.push_back(pawn_character);
		}
	      else if(pawn_character == DFA_WHITE_PAWN)
		{
		  advance_characters.push_back(DFA_WHITE_BISHOP);
		  advance_characters.push_back(DFA_WHITE_KNIGHT);
		  advance_characters.push_back(DFA_WHITE_QUEEN);
		  advance_characters.push_back(DFA_WHITE_ROOK);
		}
	      else
		{
		  advance_characters.push_back(DFA_BLACK_BISHOP);
		  advance_characters.push_back(DFA_BLACK_KNIGHT);
		  advance_characters.push_back(DFA_BLACK_QUEEN);
		  advance_characters.push_back(DFA_BLACK_ROOK);
		}

	      std::vector<shared_dfa_ptr> advance_pre_conditions = {};
	      change_vector advance_changes(64 + CHESS_SQUARE_OFFSET);
	      std::vector<shared_dfa_ptr> advance_post_conditions = {};

	      add_change(from_square,
			 pawn_character,
			 DFA_BLANK,
			 advance_pre_conditions,
			 advance_changes,
			 advance_post_conditions);

	      for(int advance_character : advance_characters)
		{
		  add_change(advance_square,
			     DFA_BLANK,
			     advance_character,
			     advance_pre_conditions,
			     advance_changes,
			     advance_post_conditions);

		  move_graph.add_edge("pawn advance from_char=" + std::to_string(pawn_character) + ", from_square=" + std::to_string(from_square) + ", to_square=" + std::to_string(advance_square) + ", to_char=" + std::to_string(advance_character),
				      "pre shared",
				      "clear en passant", // do not need "to" fanout
				      advance_pre_conditions,
				      advance_changes,
				      advance_post_conditions);

		  // pop back conditions for next advance character
		  advance_post_conditions.pop_back();
		  advance_changes[advance_layer] = change_optional();
		  advance_pre_conditions.pop_back();
		}

	      if(previous_advancement == 0)
		{
		  // initial double move

		  int double_rank = from_rank + rank_direction * 2;
		  int double_square = double_rank * 8 + from_file;

		  std::vector<shared_dfa_ptr> double_pre_conditions = {};
		  change_vector double_changes(64 + CHESS_SQUARE_OFFSET);
		  std::vector<shared_dfa_ptr> double_post_conditions = {};

		  add_change(from_square,
			     pawn_character,
			     DFA_BLANK,
			     double_pre_conditions,
			     double_changes,
			     double_post_conditions);

		  // advance square skipped over must be blank before and after
		  double_pre_conditions.push_back(DFAUtil::get_fixed(chess_shape, advance_layer, DFA_BLANK));
		  double_post_conditions.push_back(DFAUtil::get_fixed(chess_shape, advance_layer, DFA_BLANK));

		  // double square must be blank before
		  add_change(double_square,
			     DFA_BLANK,
			     en_passant_character,
			     double_pre_conditions,
			     double_changes,
			     double_post_conditions);

		  move_graph.add_edge("pawn double from_square=" + std::to_string(from_square),
				      "pre shared",
				      "clear en passant", // do not need "to" fanout
				      double_pre_conditions,
				      double_changes,
				      double_post_conditions);
		}

	      for(int capture_file = from_file - 1; capture_file <= from_file + 1; capture_file += 2)
		{
		  if((capture_file < 0) || (8 <= capture_file))
		    {
		      continue;
		    }

		  int capture_square = advance_rank * 8 + capture_file;

		  std::vector<shared_dfa_ptr> capture_pre_conditions = {};
		  change_vector capture_changes(64 + CHESS_SQUARE_OFFSET);
		  std::vector<shared_dfa_ptr> capture_post_conditions = {};

		  add_change(from_square,
			     pawn_character,
			     DFA_BLANK,
			     capture_pre_conditions,
			     capture_changes,
			     capture_post_conditions);

		  // capture constraint
		  capture_pre_conditions.push_back(get_positions_can_move_capture(side_to_move, capture_square));
		  capture_post_conditions.push_back(get_positions_can_move_capture(side_to_move, capture_square));

		  // loop over piece after advancing
		  for(int advance_character : advance_characters)
		    {
		      move_graph.add_edge("pawn capture from_square=" + std::to_string(from_square) + ", capture_square=" + std::to_string(capture_square) + ", advance_character=" + std::to_string(advance_character),
					  "pre shared",
					  get_to_node_name(advance_character, capture_square),
					  capture_pre_conditions,
					  capture_changes,
					  capture_post_conditions);
		    }

		  if(previous_advancement == 3)
		    {
		      int en_passant_square = from_rank * 8 + capture_file;

		      std::vector<shared_dfa_ptr> en_passant_pre_conditions = {};
		      change_vector en_passant_changes(64 + CHESS_SQUARE_OFFSET);
		      std::vector<shared_dfa_ptr> en_passant_post_conditions = {};

		      add_change(from_square,
				 pawn_character,
				 DFA_BLANK,
				 en_passant_pre_conditions,
				 en_passant_changes,
				 en_passant_post_conditions);

		      add_change(capture_square,
				 DFA_BLANK,
				 pawn_character,
				 en_passant_pre_conditions,
				 en_passant_changes,
				 en_passant_post_conditions);

		      add_change(en_passant_square,
				 capture_en_passant_character,
				 DFA_BLANK,
				 en_passant_pre_conditions,
				 en_passant_changes,
				 en_passant_post_conditions);

		      move_graph.add_edge("pawn en-passant from_square=" + std::to_string(from_square) + ", en_passant_square=" + std::to_string(en_passant_square),
					  "pre shared",
					  "clear en passant", // do not need "to" fanout
					  en_passant_pre_conditions,
					  en_passant_changes,
					  en_passant_post_conditions);
		    }
		}
	    }
	}
    };

  std::function<void(int, int, int, int, int)> add_castle_choice = [&](int king_character, int rook_castle_character, int rook_character, int king_from_square, int rook_from_square)
  {
    choice_vector castle_choices;

    int castle_direction = (rook_from_square > king_from_square) ? +1 : -1;
    int king_to_square = king_from_square + castle_direction * 2;
    int rook_to_square = king_from_square + castle_direction;

    std::vector<shared_dfa_ptr> castle_pre_conditions = {};
    change_vector castle_changes(64 + CHESS_SQUARE_OFFSET);
    std::vector<shared_dfa_ptr> castle_post_conditions = {};

#if CHESS_SQUARE_OFFSET == 2
    castle_changes[side_to_move] = change_type(king_from_square, king_to_square);
#endif

    // add conditions in order matching up a rook move
    add_change(rook_from_square,
	       rook_castle_character,
	       DFA_BLANK,
	       castle_pre_conditions,
	       castle_changes,
	       castle_post_conditions);

    // extra empty square for queen's side castle
    add_between_condition(rook_from_square, king_to_square, castle_pre_conditions);
    add_between_condition(rook_from_square, king_to_square, castle_post_conditions);

    add_change(king_to_square,
	       DFA_BLANK,
	       king_character,
	       castle_pre_conditions,
	       castle_changes,
	       castle_post_conditions);

    add_change(rook_to_square,
	       DFA_BLANK,
	       rook_character,
	       castle_pre_conditions,
	       castle_changes,
	       castle_post_conditions);

    add_change(king_from_square,
	       king_character,
	       DFA_BLANK,
	       castle_pre_conditions,
	       castle_changes,
	       castle_post_conditions);

    std::vector<shared_dfa_ptr> castle_threats;
    castle_threats.push_back(get_positions_threat(side_to_move, king_from_square));
    castle_threats.push_back(get_positions_threat(side_to_move, rook_to_square));
    castle_threats.push_back(get_positions_threat(side_to_move, king_to_square));

    shared_dfa_ptr castle_threat(DFAUtil::get_union_vector(chess_shape, castle_threats));
    shared_dfa_ptr no_castle_threat(DFAUtil::get_inverse(castle_threat));
    castle_pre_conditions.push_back(no_castle_threat);
    castle_post_conditions.push_back(no_castle_threat);

    move_graph.add_edge("castle king_from_square=" + std::to_string(king_from_square) + ", king_to_square=" + std::to_string(king_to_square),
			"pre shared",
			"clear castle rights",
			castle_pre_conditions,
			castle_changes,
			castle_post_conditions);
  };

  if(side_to_move == SIDE_WHITE)
    {
      add_basic_choices(DFA_WHITE_BISHOP, bishop_moves);
      add_basic_choices(DFA_WHITE_KING, king_moves);
      add_basic_choices(DFA_WHITE_KNIGHT, knight_moves);
      add_basic_choices(DFA_WHITE_QUEEN, queen_moves);
      add_basic_choices(DFA_WHITE_ROOK, rook_moves);
      add_basic_choices(DFA_WHITE_ROOK_CASTLE, rook_moves);

      add_pawn_choices(DFA_WHITE_PAWN, DFA_WHITE_PAWN_EN_PASSANT, DFA_BLACK_PAWN_EN_PASSANT, 6, -1);

      add_castle_choice(DFA_WHITE_KING, DFA_WHITE_ROOK_CASTLE, DFA_WHITE_ROOK, 60, 56);
      add_castle_choice(DFA_WHITE_KING, DFA_WHITE_ROOK_CASTLE, DFA_WHITE_ROOK, 60, 63);
    }
  else
    {
      add_basic_choices(DFA_BLACK_BISHOP, bishop_moves);
      add_basic_choices(DFA_BLACK_KING, king_moves);
      add_basic_choices(DFA_BLACK_KNIGHT, knight_moves);
      add_basic_choices(DFA_BLACK_QUEEN, queen_moves);
      add_basic_choices(DFA_BLACK_ROOK, rook_moves);
      add_basic_choices(DFA_BLACK_ROOK_CASTLE, rook_moves);

      add_pawn_choices(DFA_BLACK_PAWN, DFA_BLACK_PAWN_EN_PASSANT, DFA_WHITE_PAWN_EN_PASSANT, 1, 1);

      add_castle_choice(DFA_BLACK_KING, DFA_BLACK_ROOK_CASTLE, DFA_BLACK_ROOK, 4, 0);
      add_castle_choice(DFA_BLACK_KING, DFA_BLACK_ROOK_CASTLE, DFA_BLACK_ROOK, 4, 7);
    }

  // second phase - place pieces handling capture fanout

  for(auto to_choice : to_choices)
    {
      int to_character = std::get<0>(to_choice);
      int to_square = std::get<1>(to_choice);
      int to_layer = to_square + CHESS_SQUARE_OFFSET;

      // TODO : clear castling rights more narrowly. only need moves from starting positions to clear them.
      int to_king = (to_character == DFA_BLACK_KING) || (to_character == DFA_WHITE_KING);
      std::string next_node_name = to_king ? "clear castle rights" : "clear en passant";

      for(int prev_character = 0; prev_character < chess_shape[to_layer]; ++prev_character)
	{
	  if((prev_character == DFA_BLACK_KING) || (prev_character == DFA_WHITE_KING))
	    {
	      // do not generate king capturing moves since they can
	      // not happen in a legal game.
	      continue;
	    }

	  if(chess_is_friendly(side_to_move, prev_character))
	    {
	      // can not move over friendly pieces
	      continue;
	    }

	  std::vector<shared_dfa_ptr> to_pre_conditions;
	  change_vector to_changes(64 + CHESS_SQUARE_OFFSET);
	  std::vector<shared_dfa_ptr> to_post_conditions;

	  add_change(to_square,
		     prev_character,
		     to_character,
		     to_pre_conditions,
		     to_changes,
		     to_post_conditions);

	  move_graph.add_edge("place to_char=" + std::to_string(to_character) + ", to_square=" + std::to_string(to_square) + ", prev_char=" + std::to_string(prev_character),
			      get_to_node_name(to_character, to_square),
			      next_node_name,
			      to_pre_conditions,
			      to_changes,
			      to_post_conditions);
	}
    }

  // third phase - clear castle rights if king moved

  int rook_character = (side_to_move == SIDE_WHITE) ? DFA_WHITE_ROOK : DFA_BLACK_ROOK;
  int rook_castle_character = (side_to_move == SIDE_WHITE) ? DFA_WHITE_ROOK_CASTLE : DFA_BLACK_ROOK_CASTLE;

  int left_castle_square = (side_to_move == SIDE_WHITE) ? 56 : 0;
  int left_castle_layer = left_castle_square + CHESS_SQUARE_OFFSET;
  shared_dfa_ptr left_castle_pre_condition = DFAUtil::get_fixed(chess_shape, left_castle_layer, rook_castle_character);
  shared_dfa_ptr left_castle_post_condition = DFAUtil::get_fixed(chess_shape, left_castle_layer, rook_character);

  int right_castle_square = (side_to_move == SIDE_WHITE) ? 63 : 7;
  int right_castle_layer = right_castle_square + CHESS_SQUARE_OFFSET;
  shared_dfa_ptr right_castle_pre_condition = DFAUtil::get_fixed(chess_shape, right_castle_layer, rook_castle_character);
  shared_dfa_ptr right_castle_post_condition = DFAUtil::get_fixed(chess_shape, right_castle_layer, rook_character);

  shared_dfa_ptr no_castle_rights(new CountCharacterDFA(chess_shape, rook_castle_character, 0, 0, CHESS_SQUARE_OFFSET));

  change_vector clear_castle_none(64 + CHESS_SQUARE_OFFSET);
  move_graph.add_edge("clear castle rights - none",
		      "clear castle rights",
		      "clear en passant",
		      move_edge_condition_vector({no_castle_rights}),
		      clear_castle_none,
		      move_edge_condition_vector({no_castle_rights}));

  change_vector clear_castle_left(64 + CHESS_SQUARE_OFFSET);
  clear_castle_left[left_castle_layer] = change_type(rook_castle_character, rook_character);
  move_graph.add_edge("clear castle rights - left",
		      "clear castle rights",
		      "clear en passant",
		      move_edge_condition_vector({left_castle_pre_condition}),
		      clear_castle_left,
		      move_edge_condition_vector({no_castle_rights, left_castle_post_condition}));

  change_vector clear_castle_both(64 + CHESS_SQUARE_OFFSET);
  clear_castle_both[left_castle_layer] = change_type(rook_castle_character, rook_character);
  clear_castle_both[right_castle_layer] = change_type(rook_castle_character, rook_character);
  move_graph.add_edge("clear castle rights - both",
		      "clear castle rights",
		      "clear en passant",
		      move_edge_condition_vector({left_castle_pre_condition, right_castle_pre_condition}),
		      clear_castle_both,
		      move_edge_condition_vector({no_castle_rights, left_castle_post_condition, right_castle_post_condition}));

  change_vector clear_castle_right(64 + CHESS_SQUARE_OFFSET);
  clear_castle_right[right_castle_layer] = change_type(rook_castle_character, rook_character);
  move_graph.add_edge("clear castle rights - right",
		      "clear castle rights",
                      "clear en passant",
		      move_edge_condition_vector({right_castle_pre_condition}),
		      clear_castle_right,
		      move_edge_condition_vector({no_castle_rights, right_castle_post_condition}));

  // fourth phase - clear en passant status

  int clear_en_passant_square_min = (side_to_move == SIDE_WHITE) ? 24 : 32;
  int clear_en_passant_square_max = clear_en_passant_square_min + 8;

  int clear_en_passant_pawn_character = (side_to_move == SIDE_WHITE) ? DFA_BLACK_PAWN_EN_PASSANT : DFA_WHITE_PAWN_EN_PASSANT;
  int clear_pawn_character = (side_to_move == SIDE_WHITE) ? DFA_BLACK_PAWN : DFA_WHITE_PAWN;

  shared_dfa_ptr no_en_passant_pawns = DFAUtil::get_accept(chess_shape);
  for(int square = clear_en_passant_square_min;
      square < clear_en_passant_square_max;
      ++square)
    {
      int layer = square + CHESS_SQUARE_OFFSET;
      no_en_passant_pawns =
	DFAUtil::get_difference
	(
	 no_en_passant_pawns,
	 DFAUtil::get_fixed(chess_shape, layer, clear_en_passant_pawn_character)
	 );
    }

  std::vector<shared_dfa_ptr> clear_en_passant_pre_conditions;
  std::vector<shared_dfa_ptr> clear_en_passant_post_conditions = {no_en_passant_pawns};

  // case: no en passant status to clear
  move_graph.add_edge("no en passant pawns",
		      "clear en passant",
		      "post shared",
		      clear_en_passant_pre_conditions,
		      change_vector(64 + CHESS_SQUARE_OFFSET),
		      clear_en_passant_post_conditions);

  // case: one en passant status to clear
  for(int square = clear_en_passant_square_min;
      square < clear_en_passant_square_max;
      ++square)
    {
      int layer = square + CHESS_SQUARE_OFFSET;

      change_vector clear_en_passant_changes(64 + CHESS_SQUARE_OFFSET);
      clear_en_passant_changes[layer] = change_type(clear_en_passant_pawn_character, clear_pawn_character);

      move_graph.add_edge("clear en passant square=" + std::to_string(square),
			  "clear en passant",
			  "post shared",
			  clear_en_passant_pre_conditions,
			  clear_en_passant_changes,
			  clear_en_passant_post_conditions);
    }

  // fifth phase

  move_graph.add_edge("post shared",
		      "post shared",
		      "end",
		      move_edge_condition_vector(),
		      change_vector(64 + CHESS_SQUARE_OFFSET),
		      move_edge_condition_vector({post_shared}));

  // done

  return move_graph;
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
