// count_dfa.cpp

// internal state meanings:
// logical k => k pieces in previous squares

#include <iostream>
#include <vector>

#include "CountDFA.h"

CountDFA::CountDFA(int num_pieces)
{
  uint64_t next_states[DFA_MAX] = {0};

  add_uniform_states();

  // current layer states

  std::vector<uint64_t> current_layer_states;
  for(int i = 0; i <= 64; ++i)
    {
      current_layer_states.push_back(i == num_pieces);
    }

  // build rest of layers backwards

  for(int layer = 63; layer >= 0; --layer)
    {
      std::vector<uint64_t> previous_layer_states = current_layer_states;
      current_layer_states.clear();

      for(int previous_pieces = 0; previous_pieces <= layer; ++previous_pieces)
	{
	  next_states[0] = previous_layer_states.at(previous_pieces);
	  for(int j = 1; j < DFA_MAX; ++j)
	    {
	      next_states[j] = previous_layer_states.at(previous_pieces + 1);
	    }
	  current_layer_states.push_back(add_state(layer, next_states));
	}
    }
}
