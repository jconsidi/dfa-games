// DFAUtil.h

#ifndef DFA_UTIL_H
#define DFA_UTIL_H

#include "DFA.h"

template <int ndim, int... shape_pack>
class DFAUtil
{
 public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;

  static shared_dfa_ptr get_accept();
  static shared_dfa_ptr get_intersection(shared_dfa_ptr, shared_dfa_ptr);
  static shared_dfa_ptr get_reject();
  static shared_dfa_ptr get_union(shared_dfa_ptr, shared_dfa_ptr);
};

#endif
