// CountDFA.h

#ifndef COUNT_DFA_H
#define COUNT_DFA_H

#include "DedupedDFA.h"

class CountDFA : public DedupedDFA
{
 public:

  CountDFA(const dfa_shape_t&, int num_pieces);
};

#endif
