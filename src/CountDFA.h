// CountDFA.h

#ifndef COUNT_DFA_H
#define COUNT_DFA_H

#include "TableDFA.h"

// The strings with exactly num_pieces nonzero characters, as a view.
class CountDFA : public TableDFA
{
  static DFATable build(const dfa_shape_t&, int);

 public:

  CountDFA(const dfa_shape_t&, int num_pieces);
};

#endif
