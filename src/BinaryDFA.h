// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <map>
#include <utility>
#include <vector>

#include "DFA.h"

typedef std::pair<uint64_t, uint64_t> BinaryBuildCacheLayerKey;
typedef std::map<BinaryBuildCacheLayerKey, uint64_t> BinaryBuildCacheLayer;

struct BinaryBuildCache
{
  const DFA& left;
  const DFA& right;
  BinaryBuildCacheLayer layers[63] = {{}};
  uint64_t (*leaf_func)(uint64_t, uint64_t);

BinaryBuildCache(const DFA& left_in, const DFA& right_in, uint64_t (*leaf_func_in)(uint64_t, uint64_t))
    : left(left_in),
    right(right_in),
    leaf_func(leaf_func_in)
  {
  }
};


class BinaryDFA : public DFA
{
 protected:
  
  BinaryDFA(const DFA&, const DFA&, uint64_t (*)(uint64_t, uint64_t));
  BinaryDFA(const std::vector<const DFA *>, uint64_t (*)(uint64_t, uint64_t));

  uint64_t binary_build(int, uint64_t, uint64_t, BinaryBuildCache&);
};

#endif
