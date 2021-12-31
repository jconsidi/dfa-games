// PreviousDFA.cpp

#include "PreviousDFA.h"

#include <iostream>

#include "BetweenMasks.h"
#include "CheckDFA.h"
#include "DifferenceDFA.h"
#include "IntersectionDFA.h"
#include "FixedDFA.h"
#include "MoveSet.h"

static void get_previous_helper(std::vector<const DFA *>& output, DFACharacter moving_character, MoveSet moves, const DFA& target)
{
  std::vector<FixedDFA> blank_conditions;
  for(int i = 0; i < 64; ++i)
    {
      blank_conditions.emplace_back(i, DFA_BLANK);
    }

  for(int from_index = 0; from_index < 64; ++from_index)
    {
      for(int to_index = 0; to_index < 64; ++to_index)
	{
	  if(!(moves.moves[from_index] & (1ULL << to_index)))
	    {
	      continue;
	    }

	  FixedDFA from_condition(from_index, DFA_BLANK);
	  FixedDFA to_condition(to_index, moving_character);

	  BoardMask between_mask = between_masks.masks[from_index][to_index];

	  std::vector<const DFA *> post_conditions;
	  post_conditions.push_back(&target);
	  post_conditions.push_back(&(blank_conditions.at(from_index)));
	  for(int between_index = 0; between_index < 64; ++between_index)
	    {
	      if(between_mask & (1 << between_index))
		{
		  post_conditions.push_back(&blank_conditions.at(between_index));
		}
	    }
	  post_conditions.push_back(&to_condition);

	  IntersectionDFA post_condition(post_conditions);
	  std::cerr << "post condition has " << post_condition.states() << " states" << std::endl;

	  throw std::logic_error("get_previous_helper not implemented");
	}
    }
}

static std::vector<const DFA *> get_previous(Side side_to_move, const DFA& target)
{
  const DFA *side_to_move_in_check = CheckDFA::get_singleton(side_to_move);
  DifferenceDFA safe_target(target, *side_to_move_in_check);

  std::vector<const DFA *> output; // LATER: deal with this memory leak
  if(side_to_move == SIDE_WHITE)
    {
      get_previous_helper(output, DFA_WHITE_BISHOP, bishop_moves, safe_target);
      get_previous_helper(output, DFA_WHITE_KING, king_moves, safe_target);
      get_previous_helper(output, DFA_WHITE_KNIGHT, knight_moves, safe_target);
      get_previous_helper(output, DFA_WHITE_PAWN, pawn_advances_white, safe_target);
      get_previous_helper(output, DFA_WHITE_QUEEN, queen_moves, safe_target);
      get_previous_helper(output, DFA_WHITE_ROOK, rook_moves, safe_target);
    }
  else
    {
      get_previous_helper(output, DFA_BLACK_BISHOP, bishop_moves, safe_target);
      get_previous_helper(output, DFA_BLACK_KING, king_moves, safe_target);
      get_previous_helper(output, DFA_BLACK_KNIGHT, knight_moves, safe_target);
      get_previous_helper(output, DFA_BLACK_PAWN, pawn_advances_black, safe_target);
      get_previous_helper(output, DFA_BLACK_QUEEN, queen_moves, safe_target);
      get_previous_helper(output, DFA_BLACK_ROOK, rook_moves, safe_target);
    }

  // TODO: pawn captures, en-passant, castling

  return output;
}

PreviousDFA::PreviousDFA(Side side_to_move, const DFA& target)
  : UnionDFA(get_previous(side_to_move, target))
{
  std::cerr << "PreviousDFA(" << side_to_move << ", ...) has " << states() << " states" << std::endl;
}
