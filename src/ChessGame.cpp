// ChessGame.cpp

#include "ChessGame.h"

#include <functional>
#include <iostream>

#include "BetweenMasks.h"
#include "MoveSet.h"

typename ChessGame::shared_dfa_ptr ChessGame::get_basic_positions() const
{
  // basic position requirements:
  // * make sure initial king indexes are correct
  // * pawns in appropriate rows

  shared_dfa_ptr singleton;
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

      // TODO: en-passant pawn restrictions

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

typename ChessGame::rule_vector ChessGame::get_rules(int side_to_move) const
{
  shared_dfa_ptr basic_positions = get_basic_positions();
  shared_dfa_ptr check_positions = get_check_positions(side_to_move);
  shared_dfa_ptr not_check_positions(new inverse_dfa_type(*check_positions));

  // shared pre/post conditions for all moves
  shared_dfa_ptr pre_shared = basic_positions;
  shared_dfa_ptr post_shared(new intersection_dfa_type(*basic_positions, *not_check_positions));

  // common conditions

  std::vector<shared_dfa_ptr> blank_conditions;
  for(int square = 0; square < 64; ++square)
    {
      blank_conditions.emplace_back(new fixed_dfa_type(square + 2, DFA_BLANK));
    }

  std::vector<shared_dfa_ptr> not_friendly_conditions;
  for(int square = 0; square < 64; ++square)
    {
      std::vector<shared_dfa_ptr> square_conditions;

      if(side_to_move == SIDE_WHITE)
	{
	  for(int friendly_character = DFA_WHITE_KING;
	      friendly_character < DFA_BLACK_KING;
	      ++friendly_character)
	    {
	      square_conditions.emplace_back(new inverse_dfa_type(fixed_dfa_type(square + 2, friendly_character)));
	    }
	}
      else
	{
	  for(int friendly_character = DFA_BLACK_KING;
	      friendly_character < DFA_MAX;
	      ++friendly_character)
	    {
	      square_conditions.emplace_back(new inverse_dfa_type(fixed_dfa_type(square + 2, friendly_character)));
	    }
	}

      not_friendly_conditions.emplace_back(new intersection_dfa_type(square_conditions));
    }

  // collect rules

  rule_vector output;

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

	    std::cout << " adding rules for " << from_square << " -> " << to_square << std::endl;

	    std::vector<shared_dfa_ptr> pre_conditions = {pre_shared};
	    pre_conditions.emplace_back(new fixed_dfa_type(from_square + 2, character));
	    pre_conditions.emplace_back(not_friendly_conditions.at(to_square));

	    std::vector<shared_dfa_ptr> post_conditions = {post_shared};
	    post_conditions.emplace_back(blank_conditions[from_square]);
	    post_conditions.emplace_back(new fixed_dfa_type(to_square + 2, character));

	    // require squares between from and to squares to be empty pre and post.
	    BoardMask between_mask = between_masks.masks[from_square][to_square];
	    for(int between_square = 0; between_square < 64; ++between_square)
	      {
		if(between_mask & (1 << between_square))
		  {
		    pre_conditions.push_back(blank_conditions.at(between_square));
		    post_conditions.push_back(blank_conditions.at(between_square));
		  }
	      }

	    shared_dfa_ptr pre_condition(new intersection_dfa_type(pre_conditions));
	    shared_dfa_ptr post_condition(new intersection_dfa_type(post_conditions));

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

	      // moving piece

	      if(square == from_square)
		{
		  return (old_value == character) && (new_value == DFA_BLANK);
		}

	      if(square == to_square)
		{
		  // TODO: limit this more
		  return (new_value == character);
		}

	      // rest of board

	      return (old_value == new_value);
	    };

	    output.emplace_back(pre_condition,
				change_rule,
				post_condition);
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

      // TODO: pawn advances, pawn captures, en-passant, castling
      //add_basic_rules(DFA_WHITE_PAWN, pawn_advances_white);
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

      // TODO: pawn advances, pawn captures, en-passant, castling
      //add_basic_rules(DFA_BLACK_PAWN, pawn_advances_black);
    }

  throw std::logic_error("ChessGame::get_rules not implemented");
}