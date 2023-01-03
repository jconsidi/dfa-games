// ChangeDFA.h

#ifndef CHANGE_DFA_H
#define CHANGE_DFA_H

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "DFA.h"

typedef std::pair<int, int> change_type;
typedef std::optional<change_type> change_optional;
typedef std::vector<change_optional> change_vector;

class ChangeDFA
  : public DFA
{
public:

  ChangeDFA(const DFA&, const change_vector&);
};

#endif
