// AcceptDFA.cpp

#include "AcceptDFA.h"

AcceptDFA::AcceptDFA()
{
  uint64_t next_states[DFA_MAX];

  // penultimate state

  for(int i = 0; i < DFA_MAX; ++i)
    {
      next_states[i] = (1 << DFA_MAX) - 1;
    }
  add_state(62, next_states);

  // internal states

  for(int i = 0; i < DFA_MAX; ++i)
    {
      next_states[i] = 0;
    }
  for(int layer = 61; layer >= 0; --layer)
    {
      add_state(layer, next_states);
    }
}
