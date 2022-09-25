// ChessGame.cpp

#include "ChessGame.h"

#include <bit>
#include <functional>
#include <iostream>
#include <string>

#include "BetweenMasks.h"
#include "ChessBoardDFA.h"
#include "CountCharacterDFA.h"
#include "MoveSet.h"
#include "Profile.h"

static bool chess_default_rule(int layer, int old_value, int new_value)
{
#if CHESS_SQUARE_OFFSET == 2
  if(layer < 2)
    {
      return new_value == old_value;
    }
#endif

  // automatic piece changes

  switch(old_value)
    {
    case DFA_BLACK_PAWN_EN_PASSANT:
      return new_value == DFA_BLACK_PAWN;

    case DFA_WHITE_PAWN_EN_PASSANT:
      return new_value == DFA_WHITE_PAWN;
    }

  // no changes

  return (old_value == new_value);
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

static bool chess_pawn_maybe_promote(int side_to_move, int previous_advancement, int character)
{
  if(previous_advancement < 5)
    {
      int pawn_character = (side_to_move == SIDE_WHITE) ? DFA_WHITE_PAWN : DFA_BLACK_PAWN;
      return character == pawn_character;
    }
  assert(previous_advancement == 5);

  // promotion
  if(side_to_move == SIDE_WHITE)
    {
      switch(character)
	{
	case DFA_WHITE_BISHOP:
	case DFA_WHITE_KNIGHT:
	case DFA_WHITE_QUEEN:
	case DFA_WHITE_ROOK:
	  return true;

	default:
	  return false;
	}
    }
  else
    {
      switch(character)
	{
	case DFA_BLACK_BISHOP:
	case DFA_BLACK_KNIGHT:
	case DFA_BLACK_QUEEN:
	case DFA_BLACK_ROOK:
	  return true;

	default:
	  return false;
	}
    }
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
	std::function<void(ChessDFA *)> add_requirement = [&](ChessDFA *requirement)
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

	      add_requirement(new inverse_dfa_type(fixed_dfa_type(square + CHESS_SQUARE_OFFSET, character)));
	    }
	};

	std::function<void(int, int)> block_row = [&](int rank, int character)
	{
	  for(int file = 0; file < 8; ++file)
	    {
	      int square = rank * 8 + file;

	      add_requirement(new inverse_dfa_type(fixed_dfa_type(square + CHESS_SQUARE_OFFSET, character)));
	    }
	};

	// pawn restrictions

	block_row(0, DFA_BLACK_PAWN);
	block_row(7, DFA_BLACK_PAWN);
	block_row(0, DFA_WHITE_PAWN);
	block_row(7, DFA_WHITE_PAWN);

	// en-passant pawn restrictions

	add_requirement(new count_character_dfa_type(DFA_BLACK_PAWN_EN_PASSANT, 0, 1, CHESS_SQUARE_OFFSET));
	block_row(0, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(1, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(2, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(4, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(5, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(6, DFA_BLACK_PAWN_EN_PASSANT);
	block_row(7, DFA_BLACK_PAWN_EN_PASSANT);

	add_requirement(new count_character_dfa_type(DFA_WHITE_PAWN_EN_PASSANT, 0, 1, CHESS_SQUARE_OFFSET));
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

	return shared_dfa_ptr(new intersection_dfa_type(requirements));
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
	    square_requirements.emplace_back(new fixed_dfa_type(square + CHESS_SQUARE_OFFSET, king_character));
	    square_requirements.push_back(basic_positions); // makes union much cheaper below
	    square_requirements.push_back(get_threat_positions(checked_side, square));

	    checks.emplace_back(new intersection_dfa_type(square_requirements));
	  }

	return shared_dfa_ptr(new union_dfa_type(checks));
      });
    }

  return singletons[checked_side];
}

typename ChessGame::shared_dfa_ptr ChessGame::get_king_positions(int king_side) const
{
  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[king_side])
    {
      Profile profile(std::ostringstream() << "ChessGame::get_king_positions(" << king_side << ")");
      profile.tic("singleton");

      int king_character = (king_side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

#if CHESS_SQUARE_OFFSET == 0
      singletons[king_side] = shared_dfa_ptr(new CountCharacterDFA<CHESS_DFA_PARAMS>(king_character, 1));
#elif CHESS_SQUARE_OFFSET == 2
      std::vector<shared_dfa_ptr> king_choices;
      for(int king_square = 0; king_square < 64; ++king_square)
	{
	  std::vector<shared_dfa_ptr> king_square_requirements;
	  std::function<void(ChessDFA *)> add_requirement = [&](ChessDFA *requirement)
	  {
	    assert(requirement->size() > 0);
	    assert(requirement->states() > 0);
	    assert(requirement->ready());
	    king_square_requirements.emplace_back(requirement);
	  };

	  add_requirement(new fixed_dfa_type(king_side, king_square));

	  for(int square = 0; square < 64; ++square)
	    {
	      shared_dfa_ptr king_on_square(new fixed_dfa_type(square + CHESS_SQUARE_OFFSET, king_character));
	      assert(king_on_square->size() > 0);

	      if(square == king_square)
		{
		  king_square_requirements.push_back(king_on_square);
		}
	      else
		{
		  shared_dfa_ptr king_not_on_square(new inverse_dfa_type(*king_on_square));
		  assert(king_not_on_square->size() > 0);
		  king_square_requirements.push_back(king_not_on_square);
		}
	    }

	  king_choices.emplace_back(new intersection_dfa_type(king_square_requirements));
	  assert(king_choices[king_square]->size() > 0);
	}

      singletons[king_side] = shared_dfa_ptr(new union_dfa_type(king_choices));

      std::cout << "ChessGame::get_king_positions(" << king_side << ") => " << singletons[king_side]->size() << " positions, " << singletons[king_side]->states() << " states" << std::endl;
      assert(singletons[king_side]->size() > 0);
#else
#error
#endif
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
		threat_components.emplace_back(new fixed_dfa_type(threatening_square + CHESS_SQUARE_OFFSET, threatening_character));

		BoardMask between_mask = between_masks.masks[threatening_square][threatened_square];
		for(int between_square = std::countr_zero(between_mask); between_square < 64; between_square += 1 + std::countr_zero(between_mask >> (between_square + 1)))
		  {
		    threat_components.emplace_back(new fixed_dfa_type(between_square + CHESS_SQUARE_OFFSET, DFA_BLANK));
		  }

		threats.emplace_back(new intersection_dfa_type(threat_components));
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

	  return shared_dfa_ptr(new union_dfa_type(threats));
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

  return shared_dfa_ptr(new difference_dfa_type(*get_check_positions(side_to_move),
						*(this->get_has_moves(side_to_move))));
}

typename ChessGame::rule_vector ChessGame::get_rules_internal(int side_to_move) const
{
  rule_vector output;

  shared_dfa_ptr basic_positions = get_basic_positions();
  shared_dfa_ptr check_positions = get_check_positions(side_to_move);
  shared_dfa_ptr not_check_positions = load_or_build("not_check_positions-side=" + std::to_string(side_to_move), [=]()
  {
    return shared_dfa_ptr(new inverse_dfa_type(*check_positions));
  });

  // shared pre/post conditions for all moves
  shared_dfa_ptr pre_shared = basic_positions;
  shared_dfa_ptr post_shared = load_or_build("post_shared-side=" + std::to_string(side_to_move), [=]()
  {
    return shared_dfa_ptr(new intersection_dfa_type(*basic_positions, *not_check_positions));
  });

  // collect rules

  std::function<void(int, const MoveSet&)> add_basic_rules = [&](int character, const MoveSet& moves)
  {
    for(int from_square = 0; from_square < 64; ++from_square)
      {
	for(int to_square = 0; to_square < 64; ++to_square)
	  {
	    if(!(moves.moves[from_square] & (1ULL << to_square)))
	      {
		continue;
	      }
	    assert(to_square != from_square);

	    // require squares between from and to squares to be empty pre and post.
	    BoardMask between_mask = between_masks.masks[from_square][to_square];

	    change_func change_rule = [=](int layer, int old_value, int new_value)
	    {
#if CHESS_SQUARE_OFFSET == 2
	      if(layer == SIDE_WHITE)
		{
		  // white king index
		  if(character == DFA_WHITE_KING)
		    {
		      // moving this piece
		      return (old_value == from_square) && (new_value == to_square);
		    }
		  else
		    {
		      // moving another piece
		      return old_value == new_value;
		    }
		}

	      if(layer == SIDE_BLACK)
		{
		  // black king index
		  if(character == DFA_BLACK_KING)
		    {
		      // moving this piece
		      return (old_value == from_square) && (new_value == to_square);
		    }
		  else
		    {
		      // moving another piece
		      return old_value == new_value;
		    }
		}
#endif

	      int square = layer - CHESS_SQUARE_OFFSET;

	      // from square

	      if(square == from_square)
		{
		  return (old_value == character) && (new_value == DFA_BLANK);
		}

	      // between squares

	      if(between_mask & (1ULL << square))
		{
		  return (old_value == DFA_BLANK) && (new_value == DFA_BLANK);
		}

	      // to square

	      if(square == to_square)
		{
		  if(chess_is_friendly(side_to_move, old_value))
		    {
		      return false;
		    }

		  // specific castling losses after moving rooks

		  if(character == DFA_WHITE_ROOK_CASTLE)
		    {
		      return (new_value == DFA_WHITE_ROOK);
		    }

		  if(character == DFA_BLACK_ROOK_CASTLE)
		    {
		      return (new_value == DFA_BLACK_ROOK);
		    }

		  // default to moving piece
		  return (new_value == character);
		}

	      // all castling lost after moving king

	      if((character == DFA_WHITE_KING) && (old_value == DFA_WHITE_ROOK_CASTLE))
		{
		  return new_value == DFA_WHITE_ROOK;
		}

	      if((character == DFA_BLACK_KING) && (old_value == DFA_BLACK_ROOK_CASTLE))
		{
		  return new_value == DFA_BLACK_ROOK;
		}

	      // rest of board

	      return chess_default_rule(layer, old_value, new_value);
	    };

	    output.emplace_back(pre_shared,
				change_rule,
				post_shared,
				"basic char=" + std::to_string(character) + ", from_square=" + std::to_string(from_square) + ", to_square=" + std::to_string(to_square));
	  }
      }
  };

  std::function<void(int, int, int, int, int)> add_pawn_rules = [&](int pawn_character, int en_passant_character, int capture_en_passant_character, int rank_start, int rank_direction)
  {
    for(int from_file = 0; from_file < 8; ++from_file)
      {
	for(int previous_advancement = 0; previous_advancement < 6; ++previous_advancement)
	  {
	    int from_rank = rank_start + previous_advancement * rank_direction;
	    int from_square = from_rank * 8 + from_file;

	    int advance_rank = from_rank + rank_direction;
	    int advance_square = advance_rank * 8 + from_file;
	    change_func advance_rule = [=](int layer, int old_value, int new_value)
	    {
#if CHESS_SQUARE_OFFSET == 2
	      if(layer < 2)
		{
		  return chess_default_rule(layer, old_value, new_value);
		}
#endif

	      int square = layer - CHESS_SQUARE_OFFSET;

	      if(square == from_square)
		{
		  return (old_value == pawn_character) && (new_value == DFA_BLANK);
		}

	      if(square == advance_square)
		{
		  return ((old_value == DFA_BLANK) &&
			  chess_pawn_maybe_promote(side_to_move, previous_advancement, new_value));
		}

	      return chess_default_rule(layer, old_value, new_value);
	    };
	    output.emplace_back(pre_shared,
				advance_rule,
				post_shared,
				"pawn advance from_square=" + std::to_string(from_square) + ", advance_square=" + std::to_string(advance_square));

	    if(previous_advancement == 0)
	      {
		// initial double move

		int double_rank = from_rank + rank_direction * 2;
		int double_square = double_rank * 8 + from_file;
		change_func double_rule = [=](int layer, int old_value, int new_value)
		{
#if CHESS_SQUARE_OFFSET == 2
		  if(layer < 2)
		    {
		      return chess_default_rule(layer, old_value, new_value);
		    }
#endif

		  int square = layer - CHESS_SQUARE_OFFSET;

		  if(square == from_square)
		    {
		      return (old_value == pawn_character) && (new_value == DFA_BLANK);
		    }

		  if(square == advance_square)
		    {
		      return (old_value == DFA_BLANK) && (new_value == DFA_BLANK);
		    }

		  if(square == double_square)
		    {
		      return (old_value == DFA_BLANK) && (new_value == en_passant_character);
		    }

		  return chess_default_rule(layer, old_value, new_value);
		};
		output.emplace_back(pre_shared,
				    double_rule,
				    post_shared,
				    "pawn double from_square=" + std::to_string(from_square));
	      }

	    for(int capture_file = from_file - 1; capture_file <= from_file + 1; capture_file += 2)
	      {
		if((capture_file < 0) || (8 <= capture_file))
		  {
		    continue;
		  }

		int capture_square = advance_rank * 8 + capture_file;
		change_func capture_rule = [=](int layer, int old_value, int new_value)
		{
#if 0
		  if(layer < 2)
		    {
		      return chess_default_rule(layer, old_value, new_value);
		    }
#endif

		  int square = layer - CHESS_SQUARE_OFFSET;

		  if(square == from_square)
		    {
		      return (old_value == pawn_character) && (new_value == DFA_BLANK);
		    }

		  if(square == capture_square)
		    {
		      return (chess_is_hostile(side_to_move, old_value) &&
			      chess_pawn_maybe_promote(side_to_move, previous_advancement, new_value));
		    }

		  return chess_default_rule(layer, old_value, new_value);
		};
		output.emplace_back(pre_shared,
				    capture_rule,
				    post_shared,
				    "pawn capture from_square=" + std::to_string(from_square) + ", capture_square=" + std::to_string(capture_square));

		if(previous_advancement == 2)
		  {
		    int en_passant_square = from_rank * 8 + capture_file;
		    change_func en_passant_rule = [=](int layer, int old_value, int new_value)
		    {
#if CHESS_SQUARE_OFFSET == 2
		      if(layer < 2)
			{
			  return chess_default_rule(layer, old_value, new_value);
			}
#endif

		      int square = layer - CHESS_SQUARE_OFFSET;

		      if(square == from_square)
			{
			  return (old_value == pawn_character) && (new_value == DFA_BLANK);
			}

		      if(square == en_passant_square)
			{
			  return (old_value == capture_en_passant_character) && (new_value == DFA_BLANK);
			}

		      if(square == capture_square)
			{
			  return (old_value == DFA_BLANK) && (new_value == pawn_character);
			}

		      return chess_default_rule(layer, old_value, new_value);
		    };
		    output.emplace_back(pre_shared,
					en_passant_rule,
					post_shared,
					"pawn en-passant from_square=" + std::to_string(from_square) + ", en_passant_square=" + std::to_string(en_passant_square));
		  }
	      }
	  }
      }
  };

  std::function<void(int, int, int, int, int)> add_castle_rule = [&](int king_character, int rook_castle_character, int rook_character, int king_from_square, int rook_from_square)
  {
    int castle_direction = (rook_from_square > king_from_square) ? +1 : -1;
    int king_to_square = king_from_square + castle_direction * 2;
    int rook_to_square = king_from_square + castle_direction;
    int maybe_extra_square = king_from_square + castle_direction * 3; // rook from or blank

    change_func castle_rule = [=](int layer, int old_value, int new_value)
    {
#if CHESS_SQUARE_OFFSET == 2
      if(layer < 2)
	{
	  if((layer == 0) == (king_character == DFA_WHITE_KING))
	    {
	      return (old_value == king_from_square) && (new_value == king_to_square);
	    }

	  return chess_default_rule(layer, old_value, new_value);
	}
#endif

      int square = layer - CHESS_SQUARE_OFFSET;

      if(square == king_from_square)
	{
	  // king start
	  return (old_value == king_character) && (new_value == DFA_BLANK);
	}
      if(square == rook_to_square)
	{
	  // rook new position
	  return (old_value == DFA_BLANK) && (new_value == rook_character);
	}
      if(square == king_to_square)
	{
	  // king end
	  return (old_value == DFA_BLANK) && (new_value == king_character);
	}
      if(square == rook_from_square)
	{
	  return (old_value == rook_castle_character) && (new_value == DFA_BLANK);
	}

      // this case is inaccessible for king-side castles since it
      // matches rook from square.
      if(square == maybe_extra_square)
	{
	  return (old_value == DFA_BLANK) && (new_value == DFA_BLANK);
	}

      // remove castle eligibility from any other rooks
      if(old_value == rook_castle_character)
	{
	  return new_value == rook_character;
	}

      return chess_default_rule(layer, old_value, new_value);
    };

    std::vector<shared_dfa_ptr> castle_threats;
    castle_threats.push_back(get_threat_positions(side_to_move, king_from_square));
    castle_threats.push_back(get_threat_positions(side_to_move, rook_to_square));
    castle_threats.push_back(get_threat_positions(side_to_move, king_to_square));

    shared_dfa_ptr castle_threat(new union_dfa_type(castle_threats));
    shared_dfa_ptr pre_castle(new inverse_dfa_type(*castle_threat));

    output.emplace_back(pre_castle,
			castle_rule,
			post_shared,
			"castle king_from_square=" + std::to_string(king_from_square) + ", king_to_square=" + std::to_string(king_to_square));
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
  return output;
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
