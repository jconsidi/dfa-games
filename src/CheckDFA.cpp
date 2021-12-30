// CheckDFA.cpp

#include "CheckDFA.h"

#include <iostream>

#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "ThreatDFA.h"

std::vector<const DFA *> CheckDFA::get_king_threats(Side side_in_check)
{
  static const DFA *king_threats[2][64] = {{0}};

  DFACharacter king_character = (side_in_check == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING;

  // filters to limit to one king that could be checked.
  //
  // MAYBE: make the usage of these more efficient, but this is pretty
  // cheap already and downstream will be cached.
  static const DFA *not_kings[2][64] = {{0}};
  for(int square = 0; square < 64; ++square)
    {
      if(not_kings[side_in_check][square] == 0)
	{
	  not_kings[side_in_check][square] = new InverseDFA(FixedDFA(square, king_character));
	}
    }

  std::vector<const DFA *> output;
  for(int square = 0; square < 64; ++square)
    {
      if(king_threats[side_in_check][square] == 0)
	{
	  ThreatDFA square_threat(side_in_check, square);
	  FixedDFA square_king(square, king_character);

	  std::vector<const DFA*> threat_conditions;
	  threat_conditions.push_back(&square_threat);
	  threat_conditions.push_back(&square_king);

	  // add conditions to block multiple kings
	  for(int other_square = 0; other_square < 64; ++other_square)
	    {
	      if(other_square != square)
		{
		  threat_conditions.push_back(not_kings[side_in_check][other_square]);
		}
	    }

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
