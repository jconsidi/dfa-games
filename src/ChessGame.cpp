// ChessGame.cpp

#include "ChessGame.h"

#include <functional>
#include <iostream>

#include "BetweenMasks.h"
#include "MoveSet.h"

static bool chess_default_rule(int layer, int old_value, int new_value)
{
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
      return (DFA_WHITE_KING <= character) && (character < DFA_BLACK_KING);
    }
  else
    {
      return DFA_BLACK_KING <= character;
    }
}

static bool chess_is_hostile(int side_to_move, int character)
{
  return chess_is_friendly(1 - side_to_move, character);
}

static bool chess_pawn_maybe_promote(int side_to_move, int previous_advancement, int character)
{
  if(previous_advancement < 6)
    {
      int pawn_character = (side_to_move == SIDE_WHITE) ? DFA_WHITE_PAWN : DFA_BLACK_PAWN;
      return character == pawn_character;
    }

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

typename ChessGame::shared_dfa_ptr ChessGame::get_basic_positions() const
{
  // basic position requirements:
  // * make sure initial king indexes are correct
  // * pawns in appropriate rows

  static shared_dfa_ptr singleton;
  if(!singleton)
    {
      std::cout << "ChessGame::get_basic_positions()" << std::endl;

      std::vector<shared_dfa_ptr> requirements;
      std::function<void(ChessDFA *)> add_requirement = [&](ChessDFA *requirement)
      {
	assert(requirement->states() > 0);
	assert(requirement->ready());
	requirements.emplace_back(requirement);
      };

      std::function<void(int, int)> block_row = [&](int rank, int character)
      {
	for(int file = 0; file < 8; ++file)
	  {
	    int square = rank * 8 + file;

	    add_requirement(new inverse_dfa_type(fixed_dfa_type(square + 2, character)));
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

      // TODO: castle/rook restrictions

      // king restrictions
      requirements.push_back(get_king_positions(0));
      requirements.push_back(get_king_positions(1));

      std::cout << "get_basic_positions() => " << requirements.size() << " requirements" << std::endl;

      singleton = shared_dfa_ptr(new intersection_dfa_type(requirements));

      std::cout << "ChessGame::get_basic_positions() => " << singleton->size() << " positions, " << singleton->states() << " states" << std::endl;
    }

  return singleton;
}

typename ChessGame::shared_dfa_ptr ChessGame::get_check_positions(int side_to_move) const
{
  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[side_to_move])
    {
      std::cout << "ChessGame::get_check_positions(" << side_to_move << ")" << std::endl;

      shared_dfa_ptr king_positions = get_king_positions(side_to_move);
      int king_character = (side_to_move == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

      std::vector<shared_dfa_ptr> checks;
      for(int square = 0; square < 64; ++square)
	{
	  std::vector<shared_dfa_ptr> square_requirements;
	  square_requirements.emplace_back(new fixed_dfa_type(square + 2, king_character));
	  square_requirements.push_back(king_positions); // makes union much cheaper below
	  square_requirements.push_back(get_threat_positions(side_to_move, square));

	  checks.emplace_back(new intersection_dfa_type(square_requirements));
	}

      singletons[side_to_move] = shared_dfa_ptr(new union_dfa_type(checks));

      std::cout << "ChessGame::get_check_positions(" << side_to_move << ") => " << singletons[side_to_move]->size() << " positions, " << singletons[side_to_move]->states() << " states" << std::endl;
    }

  return singletons[side_to_move];
}

typename ChessGame::shared_dfa_ptr ChessGame::get_king_positions(int king_side) const
{
  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[king_side])
    {
      std::cout << "ChessGame::get_king_positions(" << king_side << ")" << std::endl;

      int king_character = (king_side == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

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
	      shared_dfa_ptr king_on_square(new fixed_dfa_type(square + 2, king_character));
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
    }

  return singletons[king_side];
}

typename ChessGame::shared_dfa_ptr ChessGame::get_threat_positions(int threatened_side, int threatened_square) const
{
  static shared_dfa_ptr singletons[2][64];

  if(!singletons[threatened_side][threatened_square])
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
	    threat_components.emplace_back(new fixed_dfa_type(threatening_square, threatening_character));

	    BoardMask between_mask = between_masks.masks[threatening_square][threatened_square];
	    for(int between_square = std::countr_zero(between_mask); between_square < 64; between_square += 1 + std::countr_zero(between_mask >> (between_square + 1)))
	      {
		threat_components.emplace_back(new fixed_dfa_type(between_square, DFA_BLANK));
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
	  add_threat(DFA_BLACK_QUEEN, queen_moves);
	  add_threat(DFA_BLACK_ROOK, rook_moves);
	}
      else
	{
	  add_threat(DFA_WHITE_BISHOP, bishop_moves);
	  add_threat(DFA_WHITE_KING, king_moves);
	  add_threat(DFA_WHITE_KNIGHT, knight_moves);
	  add_threat(DFA_WHITE_PAWN, pawn_captures_white);
	  add_threat(DFA_WHITE_QUEEN, queen_moves);
	  add_threat(DFA_WHITE_ROOK, rook_moves);
	}

      singletons[threatened_side][threatened_square] = shared_dfa_ptr(new union_dfa_type(threats));
    }

  return singletons[threatened_side][threatened_square];
}

typename ChessGame::shared_dfa_ptr ChessGame::get_initial_positions() const
{
  shared_dfa_ptr output(new accept_dfa_type());

  int next_square = 0;
  std::function<void(int)> set_square = [&](int character)
  {
    output = shared_dfa_ptr(new intersection_dfa_type(*output, fixed_dfa_type(next_square, character)));

    ++next_square;
  };

  set_square(60); // white king position
  set_square(4); // black king position
  // black back line
  set_square(DFA_BLACK_ROOK);
  set_square(DFA_BLACK_KNIGHT);
  set_square(DFA_BLACK_BISHOP);
  set_square(DFA_BLACK_QUEEN);
  set_square(DFA_BLACK_KING);
  set_square(DFA_BLACK_BISHOP);
  set_square(DFA_BLACK_KNIGHT);
  set_square(DFA_BLACK_ROOK);
  // black pawns
  for(int i = 0; i < 8; ++i)
    {
      set_square(DFA_BLACK_PAWN);
    }
  // empty center
  for(int i = 0; i < 32; ++i)
    {
      set_square(DFA_BLANK);
    }
  // white pawns
  for(int i = 0; i < 8; ++i)
    {
      set_square(DFA_WHITE_PAWN);
    }
  // white back line
  set_square(DFA_WHITE_ROOK);
  set_square(DFA_WHITE_KNIGHT);
  set_square(DFA_WHITE_BISHOP);
  set_square(DFA_WHITE_QUEEN);
  set_square(DFA_WHITE_KING);
  set_square(DFA_WHITE_BISHOP);
  set_square(DFA_WHITE_KNIGHT);
  set_square(DFA_WHITE_ROOK);

  assert(next_square == 66);
  assert(output->size() == 1);

  return output;
}

typename ChessGame::shared_dfa_ptr ChessGame::get_lost_positions_helper(int side_to_move) const
{
  // lost if and only if check and no legal moves

  static shared_dfa_ptr singletons[2] = {0, 0};
  if(!singletons[side_to_move])
    {
      throw std::logic_error("ChessGame::get_lost_positions_helper not implemented");
    }

  return singletons[side_to_move];
}

const typename ChessGame::rule_vector& ChessGame::get_rules(int side_to_move) const
{
  static rule_vector singletons[2] = {};

  if(!singletons[side_to_move].size())
    {
      shared_dfa_ptr basic_positions = get_basic_positions();
      shared_dfa_ptr check_positions = get_check_positions(side_to_move);
      shared_dfa_ptr not_check_positions(new inverse_dfa_type(*check_positions));

      // shared pre/post conditions for all moves
      shared_dfa_ptr pre_shared = basic_positions;
      shared_dfa_ptr post_shared(new intersection_dfa_type(*basic_positions, *not_check_positions));

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

		  int square = layer - 2;

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

		      return (new_value == character);
		    }

		  // rest of board

		  return chess_default_rule(layer, old_value, new_value);
		};

		singletons[side_to_move].emplace_back(pre_shared,
						      change_rule,
						      post_shared);
	      }
	  }
      };

      std::function<void(int, int, int, int, int)> add_pawn_rules = [&](int pawn_character, int en_passant_character, int capture_en_passant_character, int rank_start, int rank_direction)
      {
	for(int from_file = 0; from_file < 8; ++from_file)
	  {
	    for(int previous_advancement = 0; previous_advancement < 7; ++previous_advancement)
	      {
		int from_rank = rank_start + previous_advancement * rank_direction;
		int from_square = from_rank * 8 + from_file;

		int advance_rank = from_rank + rank_direction;
		int advance_square = advance_rank * 8 + from_file;
		change_func advance_rule = [=](int layer, int old_value, int new_value)
		{
		  if(layer < 2)
		    {
		      return chess_default_rule(layer, old_value, new_value);
		    }

		  int square = layer - 2;

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
		singletons[side_to_move].emplace_back(pre_shared,
                                                      advance_rule,
                                                      post_shared);

		if(previous_advancement == 0)
		  {
		    // initial double move

		    int double_rank = from_rank + rank_direction * 2;
		    int double_square = double_rank * 8 + from_file;
		    change_func double_rule = [=](int layer, int old_value, int new_value)
		    {
		      if(layer < 2)
			{
			  return chess_default_rule(layer, old_value, new_value);
			}

		      int square = layer - 2;

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
			  return ((old_value == DFA_BLANK) &&
				  chess_pawn_maybe_promote(side_to_move, previous_advancement, new_value));
			}

		      return chess_default_rule(layer, old_value, new_value);
		    };
		    singletons[side_to_move].emplace_back(pre_shared,
							  double_rule,
							  post_shared);
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
		      if(layer < 2)
			{
			  return chess_default_rule(layer, old_value, new_value);
			}

		      int square = layer - 2;

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
		    singletons[side_to_move].emplace_back(pre_shared,
                                                          capture_rule,
                                                          post_shared);

		    if(previous_advancement == 2)
		      {
			int en_passant_square = from_rank * 8 + capture_file;
			change_func en_passant_rule = [=](int layer, int old_value, int new_value)
			{
			  if(layer < 2)
			    {
			      return chess_default_rule(layer, old_value, new_value);
			    }

			  int square = layer - 2;

			  if(square == from_square)
			    {
			      return (old_value == pawn_character) && (new_value == DFA_BLANK);
			    }

			  if(square == en_passant_square)
			    {
			      return (old_value = capture_en_passant_character) && (new_value == DFA_BLANK);
			    }

			  if(square == capture_square)
			    {
			      return (old_value == DFA_BLANK) && (new_value == pawn_character);
			    }

			  return chess_default_rule(layer, old_value, new_value);
			};
			singletons[side_to_move].emplace_back(pre_shared,
							      en_passant_rule,
							      post_shared);
		      }
		  }
	      }
	  }
      };

      if(side_to_move == SIDE_WHITE)
	{
	  std::cout << "BISHOP RULES" << std::endl;
	  add_basic_rules(DFA_WHITE_BISHOP, bishop_moves);
	  std::cout << "KING RULES" << std::endl;
	  add_basic_rules(DFA_WHITE_KING, king_moves);
	  std::cout << "KNIGHT RULES" << std::endl;
	  add_basic_rules(DFA_WHITE_KNIGHT, knight_moves);
	  std::cout << "QUEEN RULES" << std::endl;
	  add_basic_rules(DFA_WHITE_QUEEN, queen_moves);
	  std::cout << "ROOK RULES" << std::endl;
	  add_basic_rules(DFA_WHITE_ROOK, rook_moves);
	  std::cout << "PAWN RULES" << std::endl;
	  add_pawn_rules(DFA_WHITE_PAWN, DFA_WHITE_PAWN_EN_PASSANT, DFA_BLACK_PAWN_EN_PASSANT, 6, -1);

	  // TODO: castling
	}
      else
	{
	  std::cout << "BISHOP RULES" << std::endl;
	  add_basic_rules(DFA_BLACK_BISHOP, bishop_moves);
	  std::cout << "KING RULES" << std::endl;
	  add_basic_rules(DFA_BLACK_KING, king_moves);
	  std::cout << "KNIGHT RULES" << std::endl;
	  add_basic_rules(DFA_BLACK_KNIGHT, knight_moves);
	  std::cout << "QUEEN RULES" << std::endl;
	  add_basic_rules(DFA_BLACK_QUEEN, queen_moves);
	  std::cout << "ROOK RULES" << std::endl;
	  add_basic_rules(DFA_BLACK_ROOK, rook_moves);
	  std::cout << "PAWN RULES" << std::endl;
	  add_pawn_rules(DFA_BLACK_PAWN, DFA_BLACK_PAWN_EN_PASSANT, DFA_WHITE_PAWN_EN_PASSANT, 1, 1);

	  // TODO: castling
	}
    }

  return singletons[side_to_move];
}
