// DNFBuilder.h

// Builds DFA representing union of intersections of DFAs.
//
// Like https://en.wikipedia.org/wiki/Disjunctive_normal_form but with DFAs instead of literals.

#ifndef DNF_BUILDER_H
#define DNF_BUILDER_H

#include <memory>
#include <vector>

#include "DFA.h"

class DNFBuilder
{
 public:

  typedef std::vector<shared_dfa_ptr> clause_type;

 private:

  dfa_shape_t shape;
  std::vector<clause_type> clauses;

  void compact(size_t);
  void compact_last(size_t);
  void compact_longest();

 public:

  DNFBuilder(const dfa_shape_t& shape_in);

  void add_clause(const clause_type&);
  shared_dfa_ptr to_dfa();
};

#endif
