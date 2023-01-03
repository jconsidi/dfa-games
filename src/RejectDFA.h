// RejectDFA.h

#ifndef REJECT_DFA_H
#define REJECT_DFA_H

#include "DFA.h"

class RejectDFA : public DFA
{
 public:

  RejectDFA(const dfa_shape_t&);
};

#endif
