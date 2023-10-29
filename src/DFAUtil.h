// DFAUtil.h

#ifndef DFA_UTIL_H
#define DFA_UTIL_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ChangeDFA.h"
#include "DFA.h"

class DFAUtil
{
public:

  static shared_dfa_ptr dedupe_by_hash(shared_dfa_ptr);
  static shared_dfa_ptr from_string(const DFAString&);
  static shared_dfa_ptr from_strings(const dfa_shape_t&, const std::vector<DFAString>&);
  static shared_dfa_ptr get_accept(const dfa_shape_t&);
  static shared_dfa_ptr get_change(shared_dfa_ptr, const change_vector&);
  static shared_dfa_ptr get_count_character(const dfa_shape_t&, int, int);
  static shared_dfa_ptr get_count_character(const dfa_shape_t&, int, int, int);
  static shared_dfa_ptr get_count_character(const dfa_shape_t&, int, int, int, int);
  static shared_dfa_ptr get_count_character(const dfa_shape_t&, int, int, int, int, int);
  static shared_dfa_ptr get_difference(shared_dfa_ptr, shared_dfa_ptr);
  static shared_dfa_ptr get_fixed(const dfa_shape_t&, int, int);
  static shared_dfa_ptr get_intersection(shared_dfa_ptr, shared_dfa_ptr);
  static shared_dfa_ptr get_intersection_vector(const dfa_shape_t&, const std::vector<shared_dfa_ptr>&);
  static shared_dfa_ptr get_inverse(shared_dfa_ptr);
  static shared_dfa_ptr get_reject(const dfa_shape_t&);
  static shared_dfa_ptr get_union(shared_dfa_ptr, shared_dfa_ptr);
  static shared_dfa_ptr get_union_vector(const dfa_shape_t&, const std::vector<shared_dfa_ptr>&);
  static shared_dfa_ptr load_by_hash(const dfa_shape_t&, std::string);
  static shared_dfa_ptr load_by_name(const dfa_shape_t&, std::string);
  static shared_dfa_ptr load_or_build(const dfa_shape_t&, std::string, std::function<shared_dfa_ptr()>);
  static std::string quick_stats(shared_dfa_ptr);
};

#endif
