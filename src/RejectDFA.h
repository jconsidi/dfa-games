// RejectDFA.h

#ifndef REJECT_DFA_H
#define REJECT_DFA_H

#include "DFA.h"

// No string, as a view: every layer has only the two reserved
// rows, and the initial state is reject.
class RejectDFA : public DFA
{
 public:

  RejectDFA(const dfa_shape_t&);
};

#endif
