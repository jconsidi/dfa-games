// CountCharacterDFA.h

#ifndef COUNT_CHARACTER_DFA_H
#define COUNT_CHARACTER_DFA_H

#include "DedupedDFA.h"

template<int ndim, int... shape_pack>
class CountCharacterDFA : public DedupedDFA<ndim, shape_pack...>
{
 public:

  CountCharacterDFA(int, int);
  CountCharacterDFA(int, int, int);
};

#endif
