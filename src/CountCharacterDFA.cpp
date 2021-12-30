// CountCharacterDFA.cpp

// internal logical states count the number of matching pieces.

#include <iostream>
#include <vector>

#include "CountCharacterDFA.h"

CountCharacterDFA::CountCharacterDFA(DFACharacter c_in, int count_in)
{
  if((count_in < 0) || 64 < count_in)
    {
      throw std::invalid_argument("count_in must be between 0 and 64 (inclusive)");
    }

  uint64_t next_states[DFA_MAX];
  std::vector<uint64_t> logical_states[63];

  // penultimate layer
  {
    int layer = 62;

    // not enough pieces to finish

    for(int i = 0; i < DFA_MAX; ++i)
      {
	next_states[i] = 0;
      }

    for(int layer_count = 0; layer_count < count_in - 2; ++layer_count)
      {
	logical_states[layer].push_back(add_state(layer, next_states));
      }

    // barely enough pieces to finish

    if(count_in >= 2)
      {
	// layer_count = count_in - 2;

	for(int i = 0; i < DFA_MAX; ++i)
	  {
	    if(i == c_in)
	      {
		// matched once, and must match again
		next_states[i] = 1 << c_in;
	      }
	    else
	      {
		// not enough since this match missed
		next_states[i] = DFA_MASK_REJECT_ALL;
	      }
	  }
	logical_states[layer].push_back(add_state(layer, next_states));
      }

    // just need one more match to finish

    if(count_in >= 1)
      {
	// layer_count = count_in - 1
	for(int i = 0; i < DFA_MAX; ++i)
	  {
	    if(i == c_in)
	      {
		// no more matches allowed
		next_states[i] = DFA_MASK_ACCEPT_ALL ^ (1 << c_in);
	      }
	    else
	      {
		// last piece must match
		next_states[i] = 1 << c_in;
	      }
	  }
	logical_states[layer].push_back(add_state(layer, next_states));
      }

    // have exactly enough matches

    if(count_in <= 62)
      {
	// layer_count = count_in
	for(int i = 0; i < DFA_MAX; ++i)
	  {
	    // no more matches allowed
	    if(i == c_in)
	      {
		next_states[i] = 0;
	      }
	    else
	      {
		next_states[i] = DFA_MASK_ACCEPT_ALL ^ (1 << c_in);
	      }
	  }
	logical_states[layer].push_back(add_state(layer, next_states));
      }

    // too many matches

    for(int i = 0; i < DFA_MAX; ++i)
      {
	// reject all
	next_states[i] = DFA_MASK_REJECT_ALL;
      }
    for(int layer_count = count_in + 1; layer_count <= 62; ++layer_count)
      {
	logical_states[layer].push_back(add_state(layer, next_states));
      }
  }

  // internal layers

  for(int layer = 61; layer >= 0; --layer)
    {
     for(int layer_count = 0; layer_count <= layer; ++layer_count)
	{
	  for(int i = 0; i < DFA_MAX; ++i)
	    {
	      next_states[i] = logical_states[layer+1].at((i == c_in) ? (layer_count + 1) : layer_count);
	    }
	  logical_states[layer].push_back(add_state(layer, next_states));
	}
    }
}
