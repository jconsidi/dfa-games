// ChangeDFA.h

#ifndef CHANGE_DFA_H
#define CHANGE_DFA_H

#include <memory>
#include <set>

#include "DFA.h"
#include "UnionDFA.h"

typedef std::function<bool(int, int, int)> change_func;

template<int ndim, int... shape_pack>
  class ChangeDFA : public DFA<ndim, shape_pack...>
{
 public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<dfa_type> shared_dfa_ptr;

private:

  uint64_t union_local(int layer, std::vector<uint64_t>& states_in);

public:

  ChangeDFA(const dfa_type&, change_func);
};

#endif
