// CheckDFA.cpp

#include "CheckDFA.h"

#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "ThreatDFA.h"

std::vector<const DFA *> CheckDFA::get_king_threats(Side side_in_check)
{
  static const DFA *king_threats[2][64] = {{0}};

  std::vector<const DFA *> output;
  for(int square = 0; square < 64; ++square)
    {
      if(king_threats[side_in_check][square] == 0)
	{
	  ThreatDFA square_threat(side_in_check, square);
	  FixedDFA square_king(square, (side_in_check == SIDE_WHITE) ? DFA_WHITE_KING : DFA_BLACK_KING);
	  king_threats[side_in_check][square] = new IntersectionDFA(square_threat, square_king);
	}

      output.push_back(king_threats[side_in_check][square]);
    }

  return output;
}

CheckDFA::CheckDFA(Side side_in_check)
  : UnionDFA(get_king_threats(side_in_check))
{
}
