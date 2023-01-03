// FixedDFA.h

#ifndef FIXED_DFA_H
#define FIXED_DFA_H

#include <vector>

#include "DedupedDFA.h"

class FixedDFA : public DedupedDFA
{
 public:
  FixedDFA(const dfa_shape_t&, int, int);
};

#endif
