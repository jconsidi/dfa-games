// UnionDFA.h

#ifndef UNION_DFA_H
#define UNION_DFA_H

#include <string>
#include <unordered_map>
#include <vector>

#include "DFA.h"

class UnionDFA : public DFA
{
  DFA *dedupe_temp;
  std::unordered_map<std::string, uint64_t> *union_cache;

  uint64_t union_internal(int, std::vector<uint64_t>);

 public:

  UnionDFA(const DFA&, const DFA&);
  UnionDFA(const std::vector<const DFA *>);
};

#endif
