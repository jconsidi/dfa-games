// DNFBuilder.h

// Builds DFA representing union of intersections of DFAs.
//
// Like https://en.wikipedia.org/wiki/Disjunctive_normal_form but with DFAs instead of literals.

#ifndef DNF_BUILDER_H
#define DNF_BUILDER_H

#include <memory>
#include <vector>

#include "DFA.h"

template <int ndim, int... shape_pack>
class DNFBuilder
{
 public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;
  typedef std::vector<shared_dfa_ptr> clause_type;

 private:

  std::vector<clause_type> clauses;

  void compact(int);
  void compact_last();
  void compact_last_two();

 public:

  DNFBuilder();

  void add_clause(const clause_type&);
  shared_dfa_ptr to_dfa();
};

#endif
