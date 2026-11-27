// CountCharacterDFA.h

#ifndef COUNT_CHARACTER_DFA_H
#define COUNT_CHARACTER_DFA_H

#include "TableDFA.h"

// The strings with between count_min and count_max copies of a character
// in layers layer_min through layer_max, as a view.
class CountCharacterDFA : public TableDFA
{
  static DFATable build(const dfa_shape_t&, int, int, int, int, int);

 public:

  CountCharacterDFA(const dfa_shape_t&, int, int);
  CountCharacterDFA(const dfa_shape_t&, int, int, int);
  CountCharacterDFA(const dfa_shape_t&, int, int, int, int);
  CountCharacterDFA(const dfa_shape_t&, int, int, int, int, int);
};

#endif
