// IntersectionManager.h

#ifndef INTERSECTION_MANAGER_H
#define INTERSECTION_MANAGER_H

#include <map>
#include <memory>

#include "IntersectionDFA.h"

template <int ndim, int... shape_pack>
  class IntersectionManager
{
 public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;

  typedef IntersectionDFA<ndim, shape_pack...> intersection_dfa_type;

 private:

  std::map<std::pair<shared_dfa_ptr,shared_dfa_ptr>, shared_dfa_ptr> intersect_cache;
  shared_dfa_ptr reject_all;

 public:

  IntersectionManager();

  shared_dfa_ptr intersect(shared_dfa_ptr, shared_dfa_ptr);
};

#endif
