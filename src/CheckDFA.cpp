// CheckDFA.cpp

#include "CheckDFA.h"

#include <iostream>

#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "LegalDFA.h"
#include "ThreatDFA.h"

std::vector<const DFA *> CheckDFA::get_king_threats(Side side_in_check)
{
  static const DFA *king_threats[2][64] = {{0}};

  LegalDFA legal;
  DFACharacter king_character = (side_in_check == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

  std::vector<const DFA *> output;
  for(int square = 0; square < 64; ++square)
    {
      if(square >= 16)
	{
	  // TODO: restore full check
	  break;
	}

      if(king_threats[side_in_check][square] == 0)
	{
	  ThreatDFA square_threat(side_in_check, square);
	  FixedDFA square_king(square, king_character);

	  std::vector<const DFA*> threat_conditions;
	  threat_conditions.push_back(&legal);
	  threat_conditions.push_back(&square_threat);
	  threat_conditions.push_back(&square_king);

	  king_threats[side_in_check][square] = new IntersectionDFA(threat_conditions);
	}

      output.push_back(king_threats[side_in_check][square]);
    }

  return output;
}

CheckDFA::CheckDFA(Side side_in_check)
  : UnionDFA(get_king_threats(side_in_check))
{
  std::cerr << "CheckDFA(" << side_in_check << ") has " << states() << " states" << std::endl;
}
