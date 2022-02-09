// ChangeDFA.h

#ifndef CHANGE_DFA_H
#define CHANGE_DFA_H

#include <map>
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

  std::vector<std::vector<std::vector<int>>> new_values_to_old_values_by_layer;
  std::vector<std::map<int, int>> change_cache; // TODO: make this a vector inside?
  std::vector<std::map<std::string, int>> union_local_cache;

  const dfa_type *dfa_temp;

  uint64_t change_state(int layer, int state_index);
  uint64_t union_local(int layer, std::vector<uint64_t>& states_in);

public:

  ChangeDFA(const dfa_type&, change_func);
};

#endif
