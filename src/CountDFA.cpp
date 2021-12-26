// count_dfa.cpp

// internal state meanings:
// 0 = reject all.
// 1+k = k pieces in previous squares.

#include <iostream>

#include "CountDFA.h"

CountDFA::CountDFA(int num_pieces)
{
  uint64_t next_states[DFA_MAX] = {0};

  // reject states
  for(int layer = 62; layer > 0; --layer)
    {
      add_state(layer, next_states);
    }

  // penultimate square
  {
    int layer = 62;

    for(int k = 0; k < num_pieces - 2; ++k)
      {
	// cannot get enough pieces
	add_state(62, next_states);
      }

    if(num_pieces >= 2)
      {
	// k = num_pieces - 2: need both squares to have pieces
	next_states[0] = 0; // cannot be no piece
	for(int j = 1; j < DFA_MAX; ++j)
	  {
	    next_states[j] = (1 << DFA_MAX) - 2; // anything but no piece
	}
	add_state(layer, next_states);
      }

    if(num_pieces >= 1)
      {
	// k = num_pieces - 1: need exactly one square to have a piece
	next_states[0] = (1 << DFA_MAX) - 2; // anything but no piece
	for(int j = 1; j < DFA_MAX; ++j)
	  {
	    next_states[j] = 1; // must be no piece
	  }
	add_state(layer, next_states);
      }

    // k = num_pieces: no more pieces allowed
    next_states[0] = 1;
    for(int j = 1; j < DFA_MAX; ++j)
      {
	next_states[j] = 0; // got a piece so reject
      }
    add_state(layer, next_states);
  }

  // build rest of layers backwards

  for(int layer = 61; layer >= 0; --layer)
    {
      for(int previous_pieces = 0; previous_pieces < num_pieces && previous_pieces <= layer; ++previous_pieces)
	{
	  next_states[0] = previous_pieces + 1; // skip reject state
	  for(int j = 1; j < DFA_MAX; ++j)
	    {
	      next_states[j] = previous_pieces + 1 + 1; // skip reject state, increment count
	    }
	  add_state(layer, next_states);
	}

      if(layer >= num_pieces)
	{
	  // at piece limit
	  next_states[0] = num_pieces + 1; // skip reject state
	  for(int j = 1; j < DFA_MAX; ++j)
	    {
	      next_states[j] = 0; // over piece limit
	    }
	  add_state(layer, next_states);
	}
    }
}
