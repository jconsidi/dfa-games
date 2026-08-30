// StringDFA.h

#ifndef STRING_DFA_H
#define STRING_DFA_H

#include <functional>

#include "DedupedDFA.h"

class StringDFA
  : public DedupedDFA
{
private:

  dfa_state_t build_internal(int, const std::vector<std::reference_wrapper<const DFAString>>&);

public:

  StringDFA(const dfa_shape_t&, const std::vector<DFAString>&);
};

#endif
