// count_dfa.cpp

// internal state meanings:
// 0 = reject all.
// 1+k = k pieces in previous squares.

#include <iostream>
#include <vector>

#include "CountDFA.h"

CountDFA::CountDFA(int num_pieces)
{
  uint64_t next_states[DFA_MAX] = {0};

  // reject states
  for(int layer = 62; layer > 0; --layer)
    {
      add_state(layer, next_states);
    }

  // current layer states

  std::vector<uint64_t> current_layer_states;
  current_layer_states.push_back(0); // reject

  // penultimate square
  {
    int layer = 62;

    for(int k = 0; k < num_pieces - 2; ++k)
      {
	// cannot get enough pieces
	current_layer_states.push_back(add_state(62, next_states));
      }

    if(num_pieces >= 2)
      {
	// k = num_pieces - 2: need both squares to have pieces
	next_states[0] = 0; // cannot be no piece
	for(int j = 1; j < DFA_MAX; ++j)
	  {
	    next_states[j] = DFA_MASK_ACCEPT_ALL ^ (1ULL << DFA_BLANK); // anything but no piece
	}
	current_layer_states.push_back(add_state(layer, next_states));
      }

    if(num_pieces >= 1)
      {
	// k = num_pieces - 1: need exactly one square to have a piece
	next_states[0] = DFA_MASK_ACCEPT_ALL ^ (1ULL << DFA_BLANK); // anything but no piece
	for(int j = 1; j < DFA_MAX; ++j)
	  {
	    next_states[j] = 1; // must be no piece
	  }
	current_layer_states.push_back(add_state(layer, next_states));
      }

    // k = num_pieces: no more pieces allowed
    next_states[0] = 1;
    for(int j = 1; j < DFA_MAX; ++j)
      {
	next_states[j] = 0; // got a piece so reject
      }
    current_layer_states.push_back(add_state(layer, next_states));
  }

  // build rest of layers backwards

  for(int layer = 61; layer >= 0; --layer)
    {
      std::vector<uint64_t> previous_layer_states = current_layer_states;
      current_layer_states.clear();
      current_layer_states.push_back(0); // reject

      for(int previous_pieces = 0; previous_pieces < num_pieces && previous_pieces <= layer; ++previous_pieces)
	{
	  next_states[0] = previous_layer_states[previous_pieces + 1]; // skip reject state
	  for(int j = 1; j < DFA_MAX; ++j)
	    {
	      next_states[j] = previous_layer_states[previous_pieces + 1 + 1]; // skip reject state, increment count
	    }
	  current_layer_states.push_back(add_state(layer, next_states));
	}

      if(layer >= num_pieces)
	{
	  // at piece limit
	  next_states[0] = previous_layer_states[num_pieces + 1]; // skip reject state
	  for(int j = 1; j < DFA_MAX; ++j)
	    {
	      next_states[j] = previous_layer_states[0]; // over piece limit
	    }
	  current_layer_states.push_back(add_state(layer, next_states));
	}
    }
}
