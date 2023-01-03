// IntersectionManager.h

// caches DFA intersections assuming that many similar sequences of DFAs will be combined...

#ifndef INTERSECTION_MANAGER_H
#define INTERSECTION_MANAGER_H

#include <memory>
#include <vector>

#include "IntersectionDFA.h"

class IntersectionManager
{
 private:

  dfa_shape_t shape;
  std::vector<shared_dfa_ptr> input_trace;
  std::vector<shared_dfa_ptr> output_trace;

 public:

  IntersectionManager(const dfa_shape_t&);

  shared_dfa_ptr intersect(shared_dfa_ptr, shared_dfa_ptr);
};

#endif
