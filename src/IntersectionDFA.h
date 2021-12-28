// IntersectionDFA.h

#ifndef INTERSECTION_DFA_H
#define INTERSECTION_DFA_H

#include <vector>

#include "BinaryDFA.h"

class IntersectionDFA : public BinaryDFA
{
 public:

  IntersectionDFA(const DFA&, const DFA&);
  IntersectionDFA(const std::vector<const DFA *>);
};

#endif
