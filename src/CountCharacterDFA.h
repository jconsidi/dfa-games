// CountCharacterDFA.h

#ifndef COUNT_CHARACTER_DFA_H
#define COUNT_CHARACTER_DFA_H

#include "DedupedDFA.h"

class CountCharacterDFA : public DedupedDFA
{
 public:

  CountCharacterDFA(const dfa_shape_t&, int, int);
  CountCharacterDFA(const dfa_shape_t&, int, int, int);
  CountCharacterDFA(const dfa_shape_t&, int, int, int, int);
  CountCharacterDFA(const dfa_shape_t&, int, int, int, int, int);
};

#endif
