// FixedDFA.h

#ifndef FIXED_DFA_H
#define FIXED_DFA_H

#include "TableDFA.h"

// The strings with fixed_character at fixed_square, as a view.
class FixedDFA : public TableDFA
{
  static DFATable build(const dfa_shape_t&, int, int);

 public:

  FixedDFA(const dfa_shape_t&, int fixed_square, int fixed_character);
};

#endif
