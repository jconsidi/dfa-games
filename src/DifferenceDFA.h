// DifferenceDFA.h

#ifndef DIFFERENCE_DFA_H
#define DIFFERENCE_DFA_H

#include "BinaryDFA.h"

class DifferenceDFA : public BinaryDFA
{
 public:

  DifferenceDFA(const DFA&, const DFA&);
};

#endif
