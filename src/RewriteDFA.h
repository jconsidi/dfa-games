// RewriteDFA.h

#ifndef REWRITE_DFA_H
#define REWRITE_DFA_H

#include <functional>

#include "DFA.h"

class RewriteDFA : public DFA
{
 public:
  RewriteDFA(const DFA&, std::function<void(int, uint64_t[DFA_MAX])>);
};

#endif
