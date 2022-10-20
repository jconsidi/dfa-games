// DFAUtil.h

#ifndef DFA_UTIL_H
#define DFA_UTIL_H

#include <memory>
#include <vector>

#include "DFA.h"

template <int ndim, int... shape_pack>
class DFAUtil
{
 public:

  typedef DFAString<ndim, shape_pack...> dfa_string_type;
  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;

private:

  static shared_dfa_ptr _singleton_if_constant(shared_dfa_ptr);

public:

  static shared_dfa_ptr from_strings(const std::vector<dfa_string_type>&);
  static shared_dfa_ptr get_accept();
  static shared_dfa_ptr get_difference(shared_dfa_ptr, shared_dfa_ptr);
  static shared_dfa_ptr get_fixed(int, int);
  static shared_dfa_ptr get_intersection(shared_dfa_ptr, shared_dfa_ptr);
  static shared_dfa_ptr get_intersection_vector(const std::vector<shared_dfa_ptr>&);
  static shared_dfa_ptr get_inverse(shared_dfa_ptr);
  static shared_dfa_ptr get_reject();
  static shared_dfa_ptr get_union(shared_dfa_ptr, shared_dfa_ptr);
  static shared_dfa_ptr get_union_vector(const std::vector<shared_dfa_ptr>&);
};

#endif
