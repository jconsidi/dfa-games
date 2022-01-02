// FixedDFA.cpp

#include "FixedDFA.h"

FixedDFA::FixedDFA(int fixed_square, DFACharacter fixed_character)
{
  // 1 state until the fixed square, then a reject state and accept
  // state until the penultimate (mask) layer.

  uint64_t next_states[64];

  // penultimate layer

  if(fixed_square == 63)
    {
      // accept/reject based on last square.

      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = (i == fixed_character);
	}
      add_state(63, next_states);
    }
  else
    {
      // already decided accept/reject in previous square and just propagating

      // reject case
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = 0;
	}
      add_state(63, next_states);

      // accept case
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = 1;
	}
      add_state(63, next_states);
    }

  // layers after fixed square (if any left)

  for(int layer = 62; layer > fixed_square; --layer)
    {
      // already decided accept/reject in previous square and just propagating

      for(int state = 0; state < 2; ++state)
	{
	  for(int i = 0; i < DFA_MAX; ++i)
	    {
	      next_states[i] = state;
	    }
	  add_state(layer, next_states);
	}
    }

  // layer with fixed square (if not last two)

  if(fixed_square < 63)
    {
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = (i == fixed_character);
	}
      add_state(fixed_square, next_states);
    }

  // layers before fixed square (if any)

  for(int i = 0; i < DFA_MAX; ++i)
    {
      next_states[i] = 0;
    }

  for(int layer = fixed_square - 1; layer >= 0; --layer)
    {
      add_state(layer, next_states);
    }
}
