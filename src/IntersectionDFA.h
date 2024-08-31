// IntersectionDFA.h

#ifndef INTERSECTION_DFA_H
#define INTERSECTION_DFA_H

#include "BinaryDFA.h"

class IntersectionDFA : public BinaryDFA
{
 public:

  IntersectionDFA(const DFA&, const DFA&);
};

#endif
