// ForcedDFA.cpp

#include "ForcedDFA.h"

#include "HasMovesDFA.h"
#include "InverseDFA.h"
#include "PreviousDFA.h"

ForcedDFA::ForcedDFA(Side side_to_move, const DFA& target_in)
  : IntersectionDFA(*(HasMovesDFA::get_singleton(side_to_move)),
		    InverseDFA(PreviousDFA(side_to_move, InverseDFA(target_in))))
{
}
