// AcceptDFA.h

#ifndef ACCEPT_DFA_H
#define ACCEPT_DFA_H

#include "DFA.h"

class AcceptDFA : public DFA
{
 public:

  AcceptDFA(const dfa_shape_t&);
};

#endif
