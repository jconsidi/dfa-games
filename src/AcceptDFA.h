// AcceptDFA.h

#ifndef ACCEPT_DFA_H
#define ACCEPT_DFA_H

#include "DFA.h"

// Every string of the shape, as a view: every layer has only the two reserved
// rows, and the initial state is accept.
class AcceptDFA : public DFA
{
 public:

  AcceptDFA(const dfa_shape_t&);
};

#endif
