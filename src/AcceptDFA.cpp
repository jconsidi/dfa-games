// AcceptDFA.cpp

#include "AcceptDFA.h"

AcceptDFA::AcceptDFA()
{
  add_uniform_states();

  // accept state at top
  uint64_t next_states[DFA_MAX];
  for(int i = 0; i < DFA_MAX; ++i)
    {
      next_states[i] = 1;
    }
  add_state(0, next_states);
}
