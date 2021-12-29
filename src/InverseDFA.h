// InverseDFA.h

#ifndef INVERSE_DFA_H
#define INVERSE_DFA_H

#include "DFA.h"

class InverseDFA : public DFA
{
 public:
  InverseDFA(const DFA&);
};

#endif
