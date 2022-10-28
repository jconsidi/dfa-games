// DNFBuilder.cpp

#include "DNFBuilder.h"

#include <iostream>
#include <stdexcept>

#include "CountManager.h"
#include "DFAUtil.h"
#include "Profile.h"

#define COMPACT_THRESHOLD 10
#define CHECK_COMPACT_THRESHOLD(clauses) \
  ( \
  (clauses.size() >= COMPACT_THRESHOLD) && \
  (clauses[clauses.size() - COMPACT_THRESHOLD].size() == clauses.back().size()) \
    )


template <int ndim, int... shape_pack>
DNFBuilder<ndim, shape_pack...>::DNFBuilder()
{
}

template <int ndim, int... shape_pack>
void DNFBuilder<ndim, shape_pack...>::add_clause(const typename DNFBuilder<ndim, shape_pack...>::clause_type& clause_in)
{
  Profile profile("add_clause");

  // trivial cases

  if(clauses.size() == 0)
    {
      clauses.push_back(clause_in);
      return;
    }

  if(clause_in.size() == 0)
    {
      // empty AND clause accepts all

      // can optimize this case by throwing out all other clauses, but
      // not expected to happen now.
      assert(false);
    }

  // compact previous clauses to maintain invariants when new clause added

  int current_clause_length = clause_in.size();

  if(clauses.size() > 0)
    {
      const clause_type& previous_clause = clauses.back();
      int previous_clause_length = previous_clause.size();

      int shared_length = 0;
      while((shared_length < previous_clause_length) &&
	    (shared_length < current_clause_length) &&
	    (previous_clause[shared_length] == clause_in[shared_length]))
	{
	  ++shared_length;
	}

      if(shared_length < previous_clause_length - 1)
	{
	  // compact the previous clauses until they match for all but
	  // the last DFA.
	  compact(shared_length + 1);
	}
    }

  std::cout << "  " << clauses.size() << " clauses after compact previous" << std::endl;

  // check invariants

  if(clauses.size() > 0)
    {
      const clause_type& previous_clause = clauses.back();
      int previous_clause_length = previous_clause.size();

      assert(previous_clause_length <= current_clause_length);

      for(int i = 0; i < previous_clause_length - 1; ++i)
	{
	  assert(previous_clause[i] == clause_in[i]);
	}
    }

  // add new clause

  clauses.push_back(clause_in);

  // trigger compaction if there are too many clauses of the same
  // length.

  if(CHECK_COMPACT_THRESHOLD(clauses))
    {
      int compact_size = clause_in.size();
      assert(compact_size > 0);
      compact_longest();
      assert(clauses.back().size() == compact_size);
    }

  std::cout << "  " << clauses.size() << " clauses after compact with new" << std::endl;

  std::cout << "  " << clauses[0].size() << "|" << clauses[0].back()->states();
  for(int i = 1; i < clauses.size(); ++i)
    {
      std::cout << ", " << clauses[i].size() << "|" << clauses[i].back()->states();
    }
  std::cout << std::endl;
}

template <int ndim, int... shape_pack>
void DNFBuilder<ndim, shape_pack...>::compact(int target_length)
{
  Profile profile("compact");

  if(target_length < 1)
    {
      throw std::range_error("target length must be at least 1");
    }

  if(clauses.size() == 0)
    {
      return;
    }

  // compact clauses at end until at most one is too long
  while((clauses.size() > 1) && (clauses.back().size() > target_length))
    {
      int second_last_size = clauses[clauses.size() - 2].size();
      if(second_last_size <= target_length)
	{
	  // only last clause needs to be compacted
	  break;
	}

      // make the last two clauses match lengths
      if(clauses.back().size() > second_last_size)
	{
	  compact_last(second_last_size);
	}
      int last_size = clauses.back().size();
      assert(last_size == second_last_size);

      // at least two clauses at the end with the same length and over
      // target... union all of them together.
      compact_longest();
    }
  assert(clauses.size() >= 1);

  if(clauses.back().size() > target_length)
    {
      assert((clauses.size() == 1) || (clauses[clauses.size() - 2].size() <= target_length));
      compact_last(target_length);
    }
  assert(clauses.back().size() <= target_length);

  // may have just increased number of clauses at the target length,
  // so maybe compact
  if(CHECK_COMPACT_THRESHOLD(clauses))
    {
      compact_longest();
    }
}

template <int ndim, int... shape_pack>
void DNFBuilder<ndim, shape_pack...>::compact_last(int target_length)
{
  Profile profile("compact_last");

  assert(target_length >= 1);

  int num_clauses = clauses.size();
  assert(num_clauses > 0);
  assert(clauses[num_clauses - 1].size() > target_length);
  if(num_clauses > 1)
    {
      assert(clauses[num_clauses - 2].size() <= target_length);
    }

  clause_type& last_clause = clauses.back();

  std::vector<shared_dfa_ptr> target_dfas;
  while(last_clause.size() >= target_length)
    {
      target_dfas.push_back(last_clause.back());
      last_clause.pop_back();
    }
  last_clause.push_back(DFAUtil<ndim, shape_pack...>::get_intersection_vector(target_dfas));
  assert(last_clause.size() == target_length);

  if(num_clauses > 1)
    {
      assert(clauses[num_clauses - 2].size() <= clauses[num_clauses - 1].size());
    }
}

template <int ndim, int... shape_pack>
void DNFBuilder<ndim, shape_pack...>::compact_longest()
{
  Profile profile("compact_longest");

  if(clauses.size() <= 1)
    {
      // no work to be done
      return;
    }

  // compact clauses tied for longest together

  int longest_size = clauses.back().size();
  assert(longest_size > 0);

  std::vector<shared_dfa_ptr> compact_todo;
  while(true)
    {
      assert(clauses.back().size() == longest_size);
      compact_todo.push_back(clauses.back().back());

      if(clauses.size() == 1)
	{
	  // last clause
	  clauses.back().pop_back();
	  break;
	}
      else if(clauses[clauses.size() - 2].size() != longest_size)
	{
	  // last clause of this size
	  clauses.back().pop_back();
	  break;
	}
      else
	{
	  // at least one more clause of this size
	  clauses.pop_back();
	}
    }

  assert(clauses.size() >= 1);
  assert(clauses.back().size() == longest_size - 1);
  clauses.back().push_back(DFAUtil<ndim, shape_pack...>::get_union_vector(compact_todo));
  assert(clauses.back().size() == longest_size);
}

template <int ndim, int... shape_pack>
typename DNFBuilder<ndim, shape_pack...>::shared_dfa_ptr DNFBuilder<ndim, shape_pack...>::to_dfa()
{
  Profile profile("to_dfa");

  // check for trivial case without clauses

  if(clauses.size() == 0)
    {
      return DFAUtil<ndim, shape_pack...>::get_reject();
    }

  // force compacting all clauses down to length 1

  compact(1);

  for(int i = 0; i < clauses.size(); ++i)
    {
      assert(clauses[i].size() == 1);
    }

  std::cout << "  " << clauses.size() << " clauses after compact" << std::endl;

  // merge all the remaining length 1 clauses

  compact_longest();
  assert(clauses.size() == 1);

  // return output

  assert(clauses[0].size() <= 1);
  if(clauses[0].size() == 1)
    {
      return clauses[0][0];
    }

  assert(clauses[0].size() == 0);
  return DFAUtil<ndim, shape_pack...>::get_accept();
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(DNFBuilder);
