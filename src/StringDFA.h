// StringDFA.h

#ifndef STRING_DFA_H
#define STRING_DFA_H

#include "DedupedDFA.h"

template <int ndim, int... shape_pack>
class StringDFA
  : public DedupedDFA<ndim, shape_pack...>
{
private:

  dfa_state_t build_internal(int, const std::vector<DFAString<ndim, shape_pack...>>&);

public:

  StringDFA(const std::vector<DFAString<ndim, shape_pack...>>&);
};

#endif
