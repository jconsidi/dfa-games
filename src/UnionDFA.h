// UnionDFA.h

#ifndef UNION_DFA_H
#define UNION_DFA_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DFA.h"

template <int ndim, int... shape_pack>
class UnionDFA : public DFA<ndim, shape_pack...>
{
  DFA<ndim, shape_pack...> *dedupe_temp;
  std::unordered_map<std::string, uint64_t> *union_cache;

  static std::vector<const DFA<ndim, shape_pack...> *> convert_inputs(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>&);
  uint64_t union_internal(int, std::vector<uint64_t>);

 public:

  UnionDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&);
  UnionDFA(const std::vector<const DFA<ndim, shape_pack...> *>&);
  UnionDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>&);
};

#endif
