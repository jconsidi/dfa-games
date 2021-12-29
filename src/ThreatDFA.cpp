// ThreatDFA.cpp

#include "ThreatDFA.h"

#include <bit>

#include "BetweenMasks.h"
#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "MoveSet.h"

static void get_threats_helper(std::vector<const DFA *>& threats, DFACharacter threatening_character, int threatened_square, MoveSet moves)
{
  BoardMask threatened_mask = 1ULL << threatened_square;

  for(int threatening_square = 0; threatening_square < 64; ++threatening_square)
    {
      if(!(moves.moves[threatening_square] & threatened_mask))
	{
	  continue;
	}

      std::vector<const DFA *> threat;
      threat.push_back(new FixedDFA(threatening_square, threatening_character));

      BoardMask between_mask = between_masks.masks[threatening_square][threatened_square];
      for(int between_square = std::countr_zero(between_mask); between_square < 64; between_square += 1 + std::countr_zero(between_mask >> (between_square + 1)))
	{
	  threat.push_back(new FixedDFA(between_square, DFA_BLANK));
	}
      assert(threat.size() > 0);

      threats.push_back(new IntersectionDFA(threat));

      // clean up temporary DFAs
      // LATER: make these singletons and skip cleanup
      for(int i = 0; i < threat.size(); ++i)
	{
	  delete threat[i];
	  threat[i] = 0;
	}
    }
}

std::vector<const DFA *> ThreatDFA::get_threats(Side threatened_side, int threatened_square)
{
  static std::vector<const DFA *> threats[2][64];

  std::vector<const DFA *>& current_threats = threats[threatened_side][threatened_square];
  if(!current_threats.size())
    {
      if(threatened_side == SIDE_WHITE)
	{
	  get_threats_helper(current_threats, DFA_BLACK_BISHOP, threatened_square, bishop_moves);
	  get_threats_helper(current_threats, DFA_BLACK_KING, threatened_square, king_moves);
	  get_threats_helper(current_threats, DFA_BLACK_KNIGHT, threatened_square, knight_moves);
	  get_threats_helper(current_threats, DFA_BLACK_PAWN, threatened_square, pawn_captures_black);
	  get_threats_helper(current_threats, DFA_BLACK_QUEEN, threatened_square, queen_moves);
	  get_threats_helper(current_threats, DFA_BLACK_ROOK, threatened_square, rook_moves);
	}
      else
	{
	  get_threats_helper(current_threats, DFA_WHITE_BISHOP, threatened_square, bishop_moves);
	  get_threats_helper(current_threats, DFA_WHITE_KING, threatened_square, king_moves);
	  get_threats_helper(current_threats, DFA_WHITE_KNIGHT, threatened_square, knight_moves);
	  get_threats_helper(current_threats, DFA_WHITE_PAWN, threatened_square, pawn_captures_white);
	  get_threats_helper(current_threats, DFA_WHITE_QUEEN, threatened_square, queen_moves);
	  get_threats_helper(current_threats, DFA_WHITE_ROOK, threatened_square, rook_moves);
	}
    }

  assert(current_threats.size() > 0);

  return current_threats;
}

ThreatDFA::ThreatDFA(Side threatened_side, int threatened_square)
  : UnionDFA(get_threats(threatened_side, threatened_square))
{
}

