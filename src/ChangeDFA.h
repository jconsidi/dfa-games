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

template<int ndim, int... shape_pack>
  class ChangeDFA : public DFA<ndim, shape_pack...>
{
public:

  ChangeDFA(const DFA<ndim, shape_pack...>&, const change_vector&);
};

#endif
