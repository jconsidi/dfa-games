// StringDFA.h

#ifndef STRING_DFA_H
#define STRING_DFA_H

#include "DedupedDFA.h"

class StringDFA
  : public DedupedDFA
{
private:

  dfa_state_t build_internal(int, const std::vector<DFAString>&);

public:

  StringDFA(const std::vector<DFAString>&);
};

#endif
