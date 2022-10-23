// DNFBuilder.cpp

#include "DNFBuilder.h"

#include <iostream>
#include <stdexcept>

#include "CountManager.h"
#include "DFAUtil.h"
#include "Profile.h"

#define COMPACT_THRESHOLD 10

template <int ndim, int... shape_pack>
DNFBuilder<ndim, shape_pack...>::DNFBuilder()
{
}

template <int ndim, int... shape_pack>
void DNFBuilder<ndim, shape_pack...>::add_clause(const typename DNFBuilder<ndim, shape_pack...>::clause_type& clause_in)
{
  Profile profile("add_clause");

  profile.tic("check trivial");

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

  profile.tic("compact previous");

  int current_clause_length = clause_in.size();

  // partially evaluate previous clauses to manage space growth
  while(clauses.size() > 0)
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

      if(shared_length >= previous_clause_length - 1)
	{
	  // the previous clause matches except maybe the last DFA
	  break;
	}

      compact(shared_length + 1);
    }

  std::cout << "  " << clauses.size() << " clauses after compact previous" << std::endl;

  // invariants

  profile.tic("check invariants");

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

  profile.tic("add new");

  clauses.push_back(clause_in);

  // partial compaction to reduce clause accumulation

  profile.tic("compact with new");

  // trigger compaction if there are too many clauses of the same
  // length.

  if((clauses.size() >= COMPACT_THRESHOLD) &&
     (clauses[clauses.size() - COMPACT_THRESHOLD].size() == clause_in.size()))
    {
      int compact_size = clause_in.size();
      assert(compact_size > 0);

      std::vector<shared_dfa_ptr> compact_todo;
      while(true)
	{
	  compact_todo.push_back(clauses.back().back());

	  if((clauses.size() >= 2) &&
	     (clauses[clauses.size() - 2].size() == compact_size))
	    {
	      // not the last clause of this size
	      clauses.pop_back();
	    }
	  else
	    {
	      // last clause of this size, so stop
	      break;
	    }
	}
      assert(clauses.size() > 0);
      assert(clauses.back().size() == compact_size);

      clauses.back().pop_back();
      clauses.back().push_back(DFAUtil<ndim, shape_pack...>::get_union_vector(compact_todo));
      assert(clauses.back().size() == compact_size);
    }

  profile.tic("stats");

  std::cout << "  " << clauses.size() << " clauses after compact with new" << std::endl;

  std::cout << "  " << clauses[0].size() << "|" << clauses[0].back()->states();
  for(int i = 1; i < clauses.size(); ++i)
    {
      std::cout << ", " << clauses[i].size() << "|" << clauses[i].back()->states();
    }
  std::cout << std::endl;

  profile.tic("done");
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

  while((clauses.size() > 1) && (clauses.back().size() > target_length))
    {
      int second_last_size = clauses[clauses.size() - 2].size();
      int last_size = clauses.back().size();

      assert(last_size >= second_last_size);

      if(last_size > second_last_size)
	{
	  compact_last(second_last_size);
	  continue;
	}
      assert(last_size == second_last_size);

      // at least two clauses at the end with the same length and over
      // target... union all of them together.

      std::vector<shared_dfa_ptr> compact_todo;
      while(true)
	{
	  assert(clauses.back().size() == last_size);
	  compact_todo.push_back(clauses.back().back());

	  if(clauses.size() == 1)
	    {
	      // last clause
	      clauses.back().pop_back();
	      break;
	    }
	  else if(clauses[clauses.size() - 2].size() != last_size)
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
      assert(clauses.back().size() == last_size - 1);
      clauses.back().push_back(DFAUtil<ndim, shape_pack...>::get_union_vector(compact_todo));
      assert(clauses.back().size() == last_size);
    }
  assert(clauses.size() >= 1);

  clause_type& last_clause = clauses.back();
  if(last_clause.size() > target_length)
    {
      assert(clauses.size() == 1);
      compact_last(target_length);
    }
  assert(clauses.back().size() <= target_length);
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

  std::vector<shared_dfa_ptr> remaining_clauses;
  while((clauses.size() > 0) && (clauses.back().size() == 1))
    {
      remaining_clauses.push_back(clauses.back().at(0));
      clauses.pop_back();
    }
  clauses.emplace_back(1, DFAUtil<ndim, shape_pack...>::get_union_vector(remaining_clauses));
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
