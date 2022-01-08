// CountCharacterDFA.h

#ifndef COUNT_CHARACTER_DFA_H
#define COUNT_CHARACTER_DFA_H

#include "DFA.h"

template<int ndim, int... shape_pack>
class CountCharacterDFA : public DFA<ndim, shape_pack...>
{
 public:

  CountCharacterDFA(int, int);
};

#endif
