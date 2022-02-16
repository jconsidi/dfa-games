// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <map>
#include <utility>
#include <vector>

#include "DFA.h"

typedef std::pair<uint64_t, uint64_t> BinaryBuildCacheLayerKey;
typedef std::map<BinaryBuildCacheLayerKey, uint64_t> BinaryBuildCacheLayer;

template <int ndim, int... shape_pack>
struct BinaryBuildCache
{
  const DFA<ndim, shape_pack...>& left;
  const DFA<ndim, shape_pack...>& right;
  BinaryBuildCacheLayer layers[ndim] = {};
  uint64_t (*leaf_func)(uint64_t, uint64_t);
  uint64_t left_sink;
  uint64_t right_sink;

  BinaryBuildCache(const DFA<ndim, shape_pack...>& left_in,
		   const DFA<ndim, shape_pack...>& right_in,
		   uint64_t (*leaf_func_in)(uint64_t, uint64_t))
    : left(left_in),
      right(right_in),
      leaf_func(leaf_func_in)
  {
    if(!left_in.ready())
      {
	throw std::logic_error("left DFA is not ready");
      }
    if(!right_in.ready())
      {
	throw std::logic_error("right DFA is not ready");
      }

    // check for left sink

    if((leaf_func(0, 0) == 0) && (leaf_func(0, 1) == 0))
      {
	left_sink = 0;
      }
    else if((leaf_func(1, 0) == 1) && (leaf_func(1, 1) == 1))
      {
	left_sink = 1;
      }
    else
      {
	// no left sink
	left_sink = ~0ULL;
      }

    // check for right sink

    if((leaf_func(0, 0) == 0) && (leaf_func(1, 0)) == 0)
      {
	right_sink = 0;
      }
    else if((leaf_func(0, 1) == 1) && (leaf_func(1, 1) == 1))
      {
	right_sink = 1;
      }
    else
      {
	// no right sink
	right_sink = ~0ULL;
      }
  }
};

template <int ndim, int... shape_pack>
class BinaryDFA : public DFA<ndim, shape_pack...>
{
  uint64_t binary_build(int, uint64_t, uint64_t, BinaryBuildCache<ndim, shape_pack...>&);

public:

  BinaryDFA(const DFA<ndim, shape_pack...>&, const DFA<ndim, shape_pack...>&, uint64_t (*)(uint64_t, uint64_t));
  BinaryDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>&, uint64_t (*)(uint64_t, uint64_t));
};

#endif
