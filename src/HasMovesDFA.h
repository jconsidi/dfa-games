// HasMovesDFA.h

#ifndef HAS_MOVES_DFA_H
#define HAS_MOVES_DFA_H

#include "PreviousDFA.h"

class HasMovesDFA : public PreviousDFA
{
  HasMovesDFA(Side);

 public:

  static const HasMovesDFA *get_singleton(Side);
};

#endif
