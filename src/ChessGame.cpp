// ChessGame.cpp

#include "ChessGame.h"

#include <bit>
#include <functional>
#include <iostream>
#include <string>

#include "BetweenMasks.h"
#include "ChessBoardDFA.h"
#include "CountCharacterDFA.h"
#include "DFAUtil.h"
#include "MoveSet.h"
#include "Profile.h"

typedef DFAUtil<CHESS_DFA_PARAMS> ChessDFAUtil;

static void add_between_condition(int from_square,
				  int to_square,
				  std::vector<typename ChessGame::shared_dfa_ptr>& conditions)
{
  // require squares between from and to squares to be empty pre and post.
  BoardMask between_mask = between_masks.masks[from_square][to_square];

  auto check_square =
    [&](int square)
    {
      if(between_mask & (1ULL << square))
	{
	  int layer = square + CHESS_SQUARE_OFFSET;
	  ChessGame::shared_dfa_ptr fixed_blank = DFAUtil<CHESS_DFA_PARAMS>::get_fixed(layer, DFA_BLANK);
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

static void add_from_condition(int from_square,
			       int from_character,
			       std::vector<typename ChessGame::shared_dfa_ptr>& pre_conditions,
			       change_vector& changes,
			       std::vector<typename ChessGame::shared_dfa_ptr>& post_conditions)
{
  int from_layer = from_square + CHESS_SQUARE_OFFSET;

  pre_conditions.push_back(DFAUtil<CHESS_DFA_PARAMS>::get_fixed(from_layer, from_character));

  assert(!changes[from_layer].has_value());
  changes[from_layer] = change_type(from_character, DFA_BLANK);

  post_conditions.push_back(DFAUtil<CHESS_DFA_PARAMS>::get_fixed(from_layer, DFA_BLANK));
}

static bool check_from(typename ChessGame::rule_type& rule,
		       int from_square, int from_character)
{
  const change_vector& changes = std::get<1>(rule);
  int from_layer = from_square + CHESS_SQUARE_OFFSET;

  if(!changes[from_layer].has_value())
    {
      return false;
    }

  const change_type& change = *(changes[from_layer]);

  return ((std::get<0>(change) == from_character) &&
	  (std::get<1>(change) == DFA_BLANK));
};

static bool check_to(typename ChessGame::rule_type& rule,
		     int to_square,
		     int to_character)
{
  const change_vector& changes = std::get<1>(rule);
  int to_layer = to_square + CHESS_SQUARE_OFFSET;

  if(!changes[to_layer].has_value())
    {
      return false;
    }

  const change_type& change = *(changes[to_layer]);

  return ((std::get<0>(change) != to_character) &&
	  (std::get<1>(change) == to_character));
};

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
  : Game("chess+" + std::to_string(CHESS_SQUARE_OFFSET))
{
}

typename ChessGame::shared_dfa_ptr ChessGame::from_board(const Board& board_in)
{
  return shared_dfa_ptr(new ChessBoardDFA(board_in));
}

typename ChessGame::shared_dfa_ptr ChessGame::get_basic_positions() const
{
  // basic position requirements:
  // * make sure initial king indexes are correct
  // * pawns in appropriate rows

  static shared_dfa_ptr singleton;
  if(!singleton)
    {
      singleton = load_or_build("basic_positions", [=]()
      {
	Profile profile("ChessGame::get_basic_positions()");
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

	      add_requirement(ChessDFAUtil::get_inverse(ChessDFAUtil::get_fixed(square + CHESS_SQUARE_OFFSET, character)));
	    }
	};

	std::function<void(int, int)> block_row = [&](int rank, int character)
	{
	  for(int file = 0; file < 8; ++file)
	    {
	      int square = rank * 8 + file;

	      add_requirement(ChessDFAUtil::get_inverse(ChessDFAUtil::get_fixed(square + CHESS_SQUARE_OFFSET, character)));
	    }
	};

	// pawn restrictions

	block_row(0, DFA_BLACK_PAWN);
	block_row(7, DFA_BLACK_PAWN);
	block_row(0, DFA_WHITE_PAWN);
	block_row(7, DFA_WHITE_PAWN);

	// en-passant pawn restrictions

	add_requirement(shared_dfa_ptr(new count_character_dfa_type(DFA_BLACK_PAWN_EN_PASSANT, 0, 1, CHESS_SQUARE_OFFSET)));
	block_row(0, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(1, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(2, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(4, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(5, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(6, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(7, DFA_BLACK_PAWN_EN_PASSANT);

	add_requirement(shared_dfa_ptr(new count_character_dfa_type(DFA_WHITE_PAWN_EN_PASSANT, 0, 1, CHESS_SQUARE_OFFSET)));
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
	requirements.push_back(get_king_positions(0));
	requirements.push_back(get_king_positions(1));

	std::cout << "get_basic_positions() => " << requirements.size() << " requirements" << std::endl;

	return ChessDFAUtil::get_intersection(requirements);
      });
    }

  return singleton;
}

typename ChessGame::shared_dfa_ptr ChessGame::get_check_positions(int checked_side) const
{
  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[checked_side])
    {
      singletons[checked_side] = load_or_build("check_positions-side=" + std::to_string(checked_side), [=]()
      {
	Profile profile(std::ostringstream() << "ChessGame::get_check_positions(" << checked_side << ")");
	profile.tic("singleton");

	shared_dfa_ptr basic_positions = get_basic_positions();
	int king_character = (checked_side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

	std::vector<shared_dfa_ptr> checks;
	for(int square = 0; square < 64; ++square)
	  {
	    std::vector<shared_dfa_ptr> square_requirements;
	    square_requirements.emplace_back(ChessDFAUtil::get_fixed(square + CHESS_SQUARE_OFFSET, king_character));
	    square_requirements.push_back(basic_positions); // makes union much cheaper below
	    square_requirements.push_back(get_threat_positions(checked_side, square));

	    checks.emplace_back(ChessDFAUtil::get_intersection(square_requirements));
	  }

	return ChessDFAUtil::get_union(checks);
      });
    }

  return singletons[checked_side];
}

typename ChessGame::shared_dfa_ptr ChessGame::get_king_positions(int king_side) const
{
  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[king_side])
    {
      singletons[king_side] = load_or_build("king_positions-side=" + std::to_string(king_side), [=]()
      {
	int king_character = (king_side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

	shared_dfa_ptr count_constraint(new CountCharacterDFA<CHESS_DFA_PARAMS>(king_character, 1, 1, CHESS_SQUARE_OFFSET));

#if CHESS_SQUARE_OFFSET == 0
	return count_constraint;
#elif CHESS_SQUARE_OFFSET == 2
	std::vector<shared_dfa_ptr> king_choices;
	for(int king_square = 0; king_square < 64; ++king_square)
	  {
	    std::vector<shared_dfa_ptr> king_square_requirements;

	    // king index
	    king_square_requirements.emplace_back(ChessDFAUtil::get_fixed(king_side, king_square));
	    // king on target square
	    king_square_requirements.emplace_back(ChessDFAUtil::get_fixed(CHESS_SQUARE_OFFSET + king_square, king_character));
	    // exactly one king
	    king_square_requirements.emplace_back(count_constraint);

	    king_choices.emplace_back(ChessDFAUtil::get_intersection(king_square_requirements));
	    assert(king_choices[king_square]->size() > 0);
	  }

	return ChessDFAUtil::get_union(king_choices);
#else
#error
#endif
      });
    }
  return singletons[king_side];
}

typename ChessGame::shared_dfa_ptr ChessGame::get_threat_positions(int threatened_side, int threatened_square) const
{
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
		threat_components.emplace_back(ChessDFAUtil::get_fixed(threatening_square + CHESS_SQUARE_OFFSET, threatening_character));

		BoardMask between_mask = between_masks.masks[threatening_square][threatened_square];
		for(int between_square = std::countr_zero(between_mask); between_square < 64; between_square += 1 + std::countr_zero(between_mask >> (between_square + 1)))
		  {
		    threat_components.emplace_back(ChessDFAUtil::get_fixed(between_square + CHESS_SQUARE_OFFSET, DFA_BLANK));
		  }

		threats.push_back(ChessDFAUtil::get_intersection(threat_components));
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

	  return ChessDFAUtil::get_union(threats);
	});
    }

  return singletons[threatened_side][threatened_square];
}

typename ChessGame::shared_dfa_ptr ChessGame::get_initial_positions_internal() const
{
  Board initial_board(INITIAL_FEN);
  return from_board(initial_board);
}

typename ChessGame::shared_dfa_ptr ChessGame::get_lost_positions_internal(int side_to_move) const
{
  // lost if and only if check and no legal moves

  return DFAUtil<CHESS_DFA_PARAMS>::get_difference(get_check_positions(side_to_move),
						   this->get_has_moves(side_to_move));
}

typename ChessGame::rule_vector ChessGame::get_rules_internal(int side_to_move) const
{
  shared_dfa_ptr basic_positions = get_basic_positions();
  shared_dfa_ptr check_positions = get_check_positions(side_to_move);
  shared_dfa_ptr not_check_positions = load_or_build("not_check_positions-side=" + std::to_string(side_to_move), [=]()
  {
    return ChessDFAUtil::get_inverse(check_positions);
  });

  // shared pre/post conditions for all moves
  shared_dfa_ptr pre_shared = basic_positions;
  shared_dfa_ptr post_shared = load_or_build("post_shared-side=" + std::to_string(side_to_move), [=]()
  {
    return ChessDFAUtil::get_intersection(basic_positions, not_check_positions);
  });

  // collect rules

  rule_vector rules_out;

  std::function<rule_vector(const rule_vector&)> handle_castle_rights =
    [](const rule_vector& rules_in)
    {
      // temporary copy as we make a lot of changes
      rule_vector rules_temp;
      for(const rule_type& rule : rules_in)
	{
	  rules_temp.push_back(rule);
	}

      rule_vector rules_out;

      // clear castling rights for rooks that move
      for(rule_type& rule : rules_temp)
	{
	  change_vector& changes = std::get<1>(rule);
	  std::string& name = std::get<3>(rule);
	  std::string name_original = name;

	  auto handle_side_castle_rights =
	    [&](int left_square, int right_square, int rook_character, int rook_castle_character)
	    {
	      // require castle rights to be gone
	      shared_dfa_ptr no_castle_rights(new CountCharacterDFA<CHESS_DFA_PARAMS>(rook_castle_character, 0, 0, CHESS_SQUARE_OFFSET));
	      std::get<2>(rule).push_back(no_castle_rights);

	      // case: no rights to clear
	      name = name_original + ", no castle rights to clear";
	      rules_out.push_back(rule);

	      // check which sides changed. unchanged positions might
	      // have castle rights to clear.

	      int left_layer = left_square + CHESS_SQUARE_OFFSET;
	      int right_layer = right_square + CHESS_SQUARE_OFFSET;

	      bool left_changed = changes[left_layer].has_value();
	      bool right_changed = changes[right_layer].has_value();

	      change_type clear_castle_right(rook_castle_character, rook_character);

	      if(!left_changed)
		{
		  // case: just clear left (queen's side) rook rights
		  changes[left_layer] = clear_castle_right;
		  name = name_original + ", clear left castle rights";
		  rules_out.push_back(rule);
		  changes[left_layer] = change_optional();
		}
	      if(!right_changed)
		{
		  // case: just clear right (king's side) rook rights
		  changes[right_layer] = clear_castle_right;
		  name = name_original + ", clear right castle rights";
		  rules_out.push_back(rule);
		  changes[right_layer] = change_optional();
		}
	      if(!left_changed && !right_changed)
		{
		  // case: clear both rook's rights
		  changes[left_layer] = clear_castle_right;
		  changes[right_layer] = clear_castle_right;
		  name = name_original + ", clear both castle rights";
		  rules_out.push_back(rule);
		  changes[right_layer] = change_optional();
		  changes[left_layer] = change_optional();
		}
	    };

	  auto handle_solo_rook =
	    [&](int rook_character, int rook_castle_character)
	    {
	      for(change_optional& change : std::get<1>(rule))
		{
		  if(change.has_value() and change->second == rook_castle_character)
		    {
		      change->second = rook_character;
		    }
		}

	      name = name_original + ", clear solo castle rights";
	      rules_out.push_back(rule);
	    };

	  if(check_from(rule, 4, DFA_BLACK_KING))
	    {
	      // black king moved from start
	      handle_side_castle_rights(0, 7, DFA_BLACK_ROOK, DFA_BLACK_ROOK_CASTLE);
	    }
	  else if(check_from(rule, 60, DFA_WHITE_KING))
	    {
	      // white king moved from start
	      handle_side_castle_rights(56, 63, DFA_WHITE_ROOK, DFA_WHITE_ROOK_CASTLE);
	    }
	  else if(check_from(rule, 0, DFA_BLACK_ROOK_CASTLE) ||
		  check_from(rule, 7, DFA_BLACK_ROOK_CASTLE))
	    {
	      // black rook with castle rights moved
	      handle_solo_rook(DFA_BLACK_ROOK, DFA_BLACK_ROOK_CASTLE);
	    }
	  else if(check_from(rule, 56, DFA_WHITE_ROOK_CASTLE) ||
		  check_from(rule, 63, DFA_WHITE_ROOK_CASTLE))
	    {
	      // white rook with castle rights moved
	      handle_solo_rook(DFA_WHITE_ROOK, DFA_WHITE_ROOK_CASTLE);
	    }
	  else
	    {
	      // not a move that would affect castle rights
	      name = name_original + ", no castle right changes";
	      rules_out.push_back(rule);
	    }
	}

      return rules_out;
    };

  // setup clearing en passant pawns

  int clear_en_passant_square_min = (side_to_move == SIDE_WHITE) ? 24 : 32;
  int clear_en_passant_square_max = clear_en_passant_square_min + 8;

  int clear_en_passant_pawn_character = (side_to_move == SIDE_WHITE) ? DFA_BLACK_PAWN_EN_PASSANT : DFA_WHITE_PAWN_EN_PASSANT;
  int clear_pawn_character = (side_to_move == SIDE_WHITE) ? DFA_BLACK_PAWN : DFA_WHITE_PAWN;

  shared_dfa_ptr no_en_passant_pawns = DFAUtil<CHESS_DFA_PARAMS>::get_accept();
  for(int square = clear_en_passant_square_min;
      square < clear_en_passant_square_max;
      ++square)
    {
      int layer = square + CHESS_SQUARE_OFFSET;
      no_en_passant_pawns =
	DFAUtil<CHESS_DFA_PARAMS>::get_difference
	(
	 no_en_passant_pawns,
	 DFAUtil<CHESS_DFA_PARAMS>::get_fixed(layer, clear_en_passant_pawn_character)
	 );
    }

  std::function<rule_vector(const rule_vector&)> handle_en_passant_pawns =
    [=](const rule_vector& rules_in)
    {
      rule_vector rules_temp;
      for(rule_type rule : rules_in)
	{
	  // add post condition that there are no en passant pawns for
	  // the other side.
	  std::get<2>(rule).push_back(no_en_passant_pawns);

	  // easy case where the no en passant pawns already
	  rules_temp.push_back(rule);

	  // check all the cases where en passant pawns need that
	  // status stripped.
	  change_vector& changes = std::get<1>(rule);

	  for(int square = clear_en_passant_square_min;
	      square < clear_en_passant_square_max;
	      ++square)
	    {
	      int layer = square + CHESS_SQUARE_OFFSET;
	      if(!changes[layer].has_value())
		{
		  // temporarily add a change clearing en passant
		  // status and add the changed rule to the output.
		  changes[layer] = change_type(clear_en_passant_pawn_character, clear_pawn_character);
		  rules_temp.push_back(rule);
		  changes[layer] = change_optional();
		}
	    }
	}
      return rules_temp;
    };

  std::function<rule_vector(const rule_vector&)> handle_promotion_pawns =
    [](const rule_vector& rules_in)
    {
      rule_vector rules_temp;

      auto add_promotion =
	[&](rule_type& rule, int promotion_square, int promotion_character)
	{
	  change_vector& changes = std::get<1>(rule);
	  int promotion_layer = promotion_square + CHESS_SQUARE_OFFSET;

	  changes[promotion_layer]->second = promotion_character;

	  rules_temp.push_back(rule);
	};

      for(rule_type rule : rules_in)
	{
	  bool found_promotion = false;

	  // check for white pawn promotions
	  for(int square = 0; square < 8; ++square)
	    {
	      if(check_to(rule, square, DFA_WHITE_PAWN))
		{
		  found_promotion = true;

		  add_promotion(rule, square, DFA_WHITE_BISHOP);
		  add_promotion(rule, square, DFA_WHITE_KNIGHT);
		  add_promotion(rule, square, DFA_WHITE_QUEEN);
		  add_promotion(rule, square, DFA_WHITE_ROOK);

		  break;
		}
	    }
	  if(found_promotion)
	    {
	      continue;
	    }

	  // check for black pawn promotions
	  for(int square = 56; square < 64; ++square)
	    {
	      if(check_to(rule, square, DFA_BLACK_PAWN))
		{
		  found_promotion = true;

		  add_promotion(rule, square, DFA_BLACK_BISHOP);
		  add_promotion(rule, square, DFA_BLACK_KNIGHT);
		  add_promotion(rule, square, DFA_BLACK_QUEEN);
		  add_promotion(rule, square, DFA_BLACK_ROOK);

		  break;
		}
	    }
	  if(found_promotion)
	    {
	      continue;
	    }

	  // no promotions found
	  rules_temp.push_back(rule);
	}
      return rules_temp;
    };

  std::function<void(const rule_vector&)> add_output =
    [&](const rule_vector& rules_in)
    {
      rule_vector rules_temp = rules_in;
      rules_temp = handle_castle_rights(rules_temp);
      rules_temp = handle_en_passant_pawns(rules_temp);
      rules_temp = handle_promotion_pawns(rules_temp);

      for(rule_type rule : rules_temp)
	{
	  rules_out.push_back(rule);
	}
    };

  std::function<void(int, const MoveSet&)> add_basic_rules =
    [&](int character, const MoveSet& moves)
    {
      rule_vector basic_rules;

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

	  for(int to_square = 0; to_square < 64; ++to_square)
	    {
	      int to_layer = to_square + CHESS_SQUARE_OFFSET;
	      int to_layer_shape = basic_positions->get_layer_shape(to_layer);

	      if(!(moves.moves[from_square] & (1ULL << to_square)))
		{
		  continue;
		}
	      assert(to_square != from_square);

	      std::vector<shared_dfa_ptr> basic_pre_conditions = {pre_shared};
	      change_vector basic_changes(64 + CHESS_SQUARE_OFFSET);
	      std::vector<shared_dfa_ptr> basic_post_conditions = {post_shared};

	      // require squares between from and to squares to be empty pre and post.
	      add_between_condition(from_square,
				    to_square,
				    basic_pre_conditions);
	      add_between_condition(from_square,
				    to_square,
				    basic_post_conditions);

	      // from conditions - will be shared across simple move and captures
	      add_from_condition(from_square,
				 character,
				 basic_pre_conditions,
				 basic_changes,
				 basic_post_conditions);

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

	      for(int prev_character = 0; prev_character < to_layer_shape; ++prev_character)
		{
		  if(!chess_is_friendly(side_to_move, prev_character))
		    {
		      basic_changes[to_layer] = change_type(prev_character, character);
		      basic_rules.emplace_back(basic_pre_conditions,
					       basic_changes,
					       basic_post_conditions,
					       "basic from_char=" + std::to_string(character) + ", from_square=" + std::to_string(from_square) + ", to_square=" + std::to_string(to_square) + ", prev_character=" + std::to_string(prev_character));

		    }
		}
	    }
	}

      add_output(basic_rules);
    };

  std::function<void(int, int, int, int, int)> add_pawn_rules =
    [&](int pawn_character, int en_passant_character, int capture_en_passant_character, int rank_start, int rank_direction)
    {
      rule_vector pawn_rules;

      for(int from_file = 0; from_file < 8; ++from_file)
	{
	  for(int previous_advancement = 0; previous_advancement < 6; ++previous_advancement)
	    {
	      int from_rank = rank_start + previous_advancement * rank_direction;
	      int from_square = from_rank * 8 + from_file;

	      int advance_rank = from_rank + rank_direction;
	      int advance_square = advance_rank * 8 + from_file;
	      int advance_layer = advance_square + CHESS_SQUARE_OFFSET;

	      std::vector<shared_dfa_ptr> advance_pre_conditions = {pre_shared};
	      change_vector advance_changes(64 + CHESS_SQUARE_OFFSET);
	      std::vector<shared_dfa_ptr> advance_post_conditions = {post_shared};

	      add_from_condition(from_square,
				 pawn_character,
				 advance_pre_conditions,
				 advance_changes,
				 advance_post_conditions);
	      advance_pre_conditions.push_back(DFAUtil<CHESS_DFA_PARAMS>::get_fixed(advance_layer, DFA_BLANK));

	      // handle_promotion_pawns will take care of promotions
	      advance_changes[advance_layer] = change_type(DFA_BLANK, pawn_character);
	      pawn_rules.emplace_back(advance_pre_conditions,
				      advance_changes,
				      advance_post_conditions,
				      "pawn advance from_char=" + std::to_string(pawn_character) + ", from_square=" + std::to_string(from_square) + ", to_square=" + std::to_string(advance_square));

	      if(previous_advancement == 0)
		{
		  // initial double move

		  int double_rank = from_rank + rank_direction * 2;
		  int double_square = double_rank * 8 + from_file;
		  int double_layer = double_square + CHESS_SQUARE_OFFSET;

		  std::vector<shared_dfa_ptr> double_pre_conditions = {pre_shared};
		  change_vector double_changes(64 + CHESS_SQUARE_OFFSET);
		  std::vector<shared_dfa_ptr> double_post_conditions = {post_shared};

		  add_from_condition(from_square,
				     pawn_character,
				     double_pre_conditions,
				     double_changes,
				     double_post_conditions);

		  // advance square skipped over must be blank before and after
		  double_pre_conditions.push_back(DFAUtil<CHESS_DFA_PARAMS>::get_fixed(advance_layer, DFA_BLANK));
		  double_post_conditions.push_back(DFAUtil<CHESS_DFA_PARAMS>::get_fixed(advance_layer, DFA_BLANK));

		  // double square must be blank before
		  double_pre_conditions.push_back(DFAUtil<CHESS_DFA_PARAMS>::get_fixed(double_layer, DFA_BLANK));

		  double_changes[double_layer] = change_type(DFA_BLANK, en_passant_character);
		  pawn_rules.emplace_back(double_pre_conditions,
					  double_changes,
					  double_post_conditions,
					  "pawn double from_square=" + std::to_string(from_square));
		}

	      for(int capture_file = from_file - 1; capture_file <= from_file + 1; capture_file += 2)
		{
		  if((capture_file < 0) || (8 <= capture_file))
		    {
		      continue;
		    }

		  int capture_square = advance_rank * 8 + capture_file;
		  int capture_layer = capture_square + CHESS_SQUARE_OFFSET;
		  int capture_layer_shape = basic_positions->get_layer_shape(capture_layer);

		  std::vector<shared_dfa_ptr> capture_pre_conditions = {pre_shared};
		  change_vector capture_changes(64 + CHESS_SQUARE_OFFSET);
		  std::vector<shared_dfa_ptr> capture_post_conditions = {post_shared};

		  add_from_condition(from_square,
				     pawn_character,
				     capture_pre_conditions,
				     capture_changes,
				     capture_post_conditions);

		  // loop over captured pieces
		  for(int prev_character = 0; prev_character < capture_layer_shape; ++prev_character)
		    {
		      if(chess_is_hostile(side_to_move, prev_character))
			{
			  capture_changes[capture_layer] = change_type(prev_character, pawn_character);
			  pawn_rules.emplace_back(capture_pre_conditions,
						  capture_changes,
						  capture_post_conditions,
						  "pawn capture from_square=" + std::to_string(from_square) + ", capture_square=" + std::to_string(capture_square) + ", prev_character=" + std::to_string(prev_character));
			}
		    }

		  if(previous_advancement == 3)
		    {
		      int en_passant_square = from_rank * 8 + capture_file;
		      int en_passant_layer = en_passant_square + CHESS_SQUARE_OFFSET;

		      std::vector<shared_dfa_ptr> en_passant_pre_conditions = {pre_shared};
		      change_vector en_passant_changes(64 + CHESS_SQUARE_OFFSET);
		      std::vector<shared_dfa_ptr> en_passant_post_conditions = {post_shared};

		      add_from_condition(from_square,
					 pawn_character,
					 en_passant_pre_conditions,
					 en_passant_changes,
					 en_passant_post_conditions);

		      en_passant_changes[en_passant_layer] = change_type(capture_en_passant_character, DFA_BLANK);
		      en_passant_changes[capture_layer] = change_type(DFA_BLANK, pawn_character);

		      pawn_rules.emplace_back(en_passant_pre_conditions,
					      en_passant_changes,
					      en_passant_post_conditions,
					      "pawn en-passant from_square=" + std::to_string(from_square) + ", en_passant_square=" + std::to_string(en_passant_square));
		    }
		}
	    }
	}

      add_output(pawn_rules);
    };

  std::function<void(int, int, int, int, int)> add_castle_rule = [&](int king_character, int rook_castle_character, int rook_character, int king_from_square, int rook_from_square)
  {
    rule_vector castle_rules;

    int castle_direction = (rook_from_square > king_from_square) ? +1 : -1;
    int king_to_square = king_from_square + castle_direction * 2;
    int rook_to_square = king_from_square + castle_direction;

    std::vector<shared_dfa_ptr> castle_pre_conditions = {pre_shared};
    change_vector castle_changes(64 + CHESS_SQUARE_OFFSET);
    std::vector<shared_dfa_ptr> castle_post_conditions = {post_shared};

    // add conditions in order matching up a rook move
    add_from_condition(rook_from_square,
		       rook_castle_character,
		       castle_pre_conditions,
		       castle_changes,
		       castle_post_conditions);
    add_between_condition(rook_from_square, king_from_square, castle_pre_conditions);
    add_between_condition(rook_from_square, king_to_square, castle_post_conditions);
    add_from_condition(king_from_square,
		       king_character,
		       castle_pre_conditions,
		       castle_changes,
		       castle_post_conditions);

#if CHESS_SQUARE_OFFSET == 2
    castle_changes[side_to_move] = change_type(king_from_square, king_to_square);
#endif

    castle_changes[king_from_square + CHESS_SQUARE_OFFSET] = change_type(king_character, DFA_BLANK);
    castle_changes[king_to_square + CHESS_SQUARE_OFFSET] = change_type(DFA_BLANK, king_character);
    castle_changes[rook_to_square + CHESS_SQUARE_OFFSET] = change_type(DFA_BLANK, rook_character);

    std::vector<shared_dfa_ptr> castle_threats;
    castle_threats.push_back(get_threat_positions(side_to_move, king_from_square));
    castle_threats.push_back(get_threat_positions(side_to_move, rook_to_square));
    castle_threats.push_back(get_threat_positions(side_to_move, king_to_square));

    shared_dfa_ptr castle_threat(ChessDFAUtil::get_union(castle_threats));
    shared_dfa_ptr no_castle_threat(ChessDFAUtil::get_inverse(castle_threat));
    castle_pre_conditions.push_back(no_castle_threat);
    castle_post_conditions.push_back(no_castle_threat);

    castle_rules.emplace_back(castle_pre_conditions,
			      castle_changes,
			      castle_post_conditions,
			      "castle king_from_square=" + std::to_string(king_from_square) + ", king_to_square=" + std::to_string(king_to_square));

    add_output(castle_rules);
  };

  if(side_to_move == SIDE_WHITE)
    {
      add_basic_rules(DFA_WHITE_BISHOP, bishop_moves);
      add_basic_rules(DFA_WHITE_KING, king_moves);
      add_basic_rules(DFA_WHITE_KNIGHT, knight_moves);
      add_basic_rules(DFA_WHITE_QUEEN, queen_moves);
      add_basic_rules(DFA_WHITE_ROOK, rook_moves);
      add_basic_rules(DFA_WHITE_ROOK_CASTLE, rook_moves);

      add_pawn_rules(DFA_WHITE_PAWN, DFA_WHITE_PAWN_EN_PASSANT, DFA_BLACK_PAWN_EN_PASSANT, 6, -1);

      add_castle_rule(DFA_WHITE_KING, DFA_WHITE_ROOK_CASTLE, DFA_WHITE_ROOK, 60, 56);
      add_castle_rule(DFA_WHITE_KING, DFA_WHITE_ROOK_CASTLE, DFA_WHITE_ROOK, 60, 63);
    }
  else
    {
      add_basic_rules(DFA_BLACK_BISHOP, bishop_moves);
      add_basic_rules(DFA_BLACK_KING, king_moves);
      add_basic_rules(DFA_BLACK_KNIGHT, knight_moves);
      add_basic_rules(DFA_BLACK_QUEEN, queen_moves);
      add_basic_rules(DFA_BLACK_ROOK, rook_moves);
      add_basic_rules(DFA_BLACK_ROOK_CASTLE, rook_moves);

      add_pawn_rules(DFA_BLACK_PAWN, DFA_BLACK_PAWN_EN_PASSANT, DFA_WHITE_PAWN_EN_PASSANT, 1, 1);

      add_castle_rule(DFA_BLACK_KING, DFA_BLACK_ROOK_CASTLE, DFA_BLACK_ROOK, 4, 0);
      add_castle_rule(DFA_BLACK_KING, DFA_BLACK_ROOK_CASTLE, DFA_BLACK_ROOK, 4, 7);
    }

  return rules_out;
}

std::string ChessGame::position_to_string(const dfa_string_type& position_in) const
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

  // assume white's move
  fen_temp += " w";

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

  Board board(fen_temp);
  return board.to_string();
}
