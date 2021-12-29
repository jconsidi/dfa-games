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
      // accept/reject based on last square, so penultimate layer sets
      // up masks to check that square.

      uint64_t last_mask = 1 << fixed_character;

      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = last_mask;
	}
      add_state(62, next_states);
    }
  else if(fixed_square == 62)
    {
      // one state splitting into accept/reject based on this square.
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = (i == fixed_character) ? DFA_MASK_ACCEPT_ALL : DFA_MASK_REJECT_ALL;
	}
      add_state(62, next_states);
    }
  else
    {
      // already decided accept/reject in previous square and just propagating

      // reject case
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = DFA_MASK_REJECT_ALL;
	}
      add_state(62, next_states);

      // accept case
      for(int i = 0; i < DFA_MAX; ++i)
	{
	  next_states[i] = DFA_MASK_ACCEPT_ALL;
	}
      add_state(62, next_states);
    }

  // layers after fixed square (if any left)

  for(int layer = 61; layer > fixed_square; --layer)
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

  if(fixed_square < 62)
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
