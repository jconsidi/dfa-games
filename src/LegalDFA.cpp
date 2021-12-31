// LegalDFA.cpp

#include "LegalDFA.h"

#include <iostream>

#include "CountCharacterDFA.h"
#include "FixedDFA.h"
#include "InverseDFA.h"

std::vector<const DFA *> LegalDFA::get_legal_conditions()
{
  static std::vector<const DFA *> output;
  if(output.size() == 0)
    {
      // forbid pawns in H rank
      for(int square = 0; square < 8; ++square)
	{
	  output.push_back(new InverseDFA(FixedDFA(square, DFA_BLACK_PAWN)));
	  output.push_back(new InverseDFA(FixedDFA(square, DFA_WHITE_PAWN)));
	}

      // forbid pawns in A rank
      for(int square = 48; square < 64; ++square)
	{
	  output.push_back(new InverseDFA(FixedDFA(square, DFA_BLACK_PAWN)));
	  output.push_back(new InverseDFA(FixedDFA(square, DFA_WHITE_PAWN)));
	}

      // one king per side
      output.push_back(new CountCharacterDFA(DFA_BLACK_KING, 1));
      output.push_back(new CountCharacterDFA(DFA_WHITE_KING, 1));
    }

  return output;
}

LegalDFA::LegalDFA()
  : IntersectionDFA(get_legal_conditions())
{
  std::cerr << "LegalDFA() has " << states() << " states" << std::endl;
}
