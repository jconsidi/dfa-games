// ReverseMoveDFA.cpp

#include "ReverseMoveDFA.h"

ReverseMoveDFA::ReverseMoveDFA(const DFA& dfa_in, DFACharacter c_in, int from_index, int to_index)
  : RewriteDFA(dfa_in, [=](int layer_rewrite, uint64_t states_rewrite[DFA_MAX]) -> void
  {
    if(layer_rewrite == from_index)
      {
	// replacing blank square with c_in
	states_rewrite[c_in] = states_rewrite[DFA_BLANK];
	states_rewrite[DFA_BLANK] = 0; // reject
      }
    // TODO: pawn capture/non-capture rules, en-passant, castling
    else if(layer_rewrite == to_index)
      {
	uint64_t to_state = states_rewrite[c_in];

	// this piece was not here before the move.
	states_rewrite[c_in] = 0;

	// no capture option
	states_rewrite[DFA_BLANK] = to_state;

	// capture choices
	if((DFA_WHITE_KING <= c_in) && (c_in < DFA_BLACK_KING))
	  {
	    // white is moving.
	    for(int i = DFA_BLACK_KING + 1; i < DFA_MAX; ++i)
	      {
		states_rewrite[i] = to_state;
	      }
	  }
	else if(DFA_WHITE_KING <= c_in)
	  {
	    // black is moving.
	    for(int i = DFA_WHITE_KING + 1; i < DFA_BLACK_KING; ++i)
	      {
		states_rewrite[i] = to_state;
	      }
	  }
	else
	  {
	    // should not get here unless DFA_BLACK_KING is not the
	    // first non-blank choice.
	    assert(false);
	  }
      }
    else
      {
	// no changes
      }
  })
{
}
