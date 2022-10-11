// IntersectionManager.h

// caches DFA intersections assuming that many similar sequences of DFAs will be combined...

#ifndef INTERSECTION_MANAGER_H
#define INTERSECTION_MANAGER_H

#include <memory>
#include <vector>

#include "IntersectionDFA.h"

template <int ndim, int... shape_pack>
  class IntersectionManager
{
 public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;

  typedef IntersectionDFA<ndim, shape_pack...> intersection_dfa_type;

 private:

  std::vector<shared_dfa_ptr> input_trace;
  std::vector<shared_dfa_ptr> output_trace;

 public:

  IntersectionManager();

  shared_dfa_ptr intersect(shared_dfa_ptr, shared_dfa_ptr);
};

#endif
