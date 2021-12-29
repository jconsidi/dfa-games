// ForcedDFA.cpp

#include "ForcedDFA.h"

#include "InverseDFA.h"
#include "PreviousDFA.h"
#include "Singleton.h"

ForcedDFA::ForcedDFA(Side side_to_move, const DFA& target_in)
  : IntersectionDFA(*(Singleton::get_has_moves(side_to_move)),
		    InverseDFA(PreviousDFA(side_to_move, InverseDFA(target_in))))
{
}
