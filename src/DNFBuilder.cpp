// DNFBuilder.cpp

#include "DNFBuilder.h"

#include <iostream>
#include <stdexcept>

#include "CountManager.h"
#include "DFAUtil.h"
#include "Profile.h"

static CountManager count_manager("DNFBuilder");

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

  // ensure that the last clause is longer than the previous one, or
  // has fewer states. this gives a handwavy logarithmic bound on the
  // number of clauses per clause length.
  while((clauses.size() >= 2) &&
	(clauses[clauses.size() - 2].size() == clauses[clauses.size() - 1].size()) &&
	(clauses[clauses.size() - 2].back()->states() <= clauses[clauses.size() - 1].back()->states()))
    {
      profile.tic("compact_last_two");
      compact_last_two();
    }

  std::cout << "  " << clauses.size() << " clauses after compact with new" << std::endl;

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
      clause_type& second_last = clauses[clauses.size() - 2];
      clause_type& last_clause = clauses.back();

      assert(last_clause.size() >= second_last.size());

      if(last_clause.size() > second_last.size())
	{
	  compact_last();
	  continue;
	}
      assert(last_clause.size() == second_last.size());

      // merge last two clauses
      compact_last_two();
    }
  assert(clauses.size() >= 1);

  clause_type& last_clause = clauses.back();
  while(last_clause.size() > target_length)
    {
      assert(clauses.size() == 1);
      compact_last();
    }
}

template <int ndim, int... shape_pack>
void DNFBuilder<ndim, shape_pack...>::compact_last()
{
  Profile profile("compact_last");

  int num_clauses = clauses.size();
  if(num_clauses > 1)
    {
      assert(clauses[num_clauses - 2].size() < clauses[num_clauses - 1].size());
    }

  clause_type& last_clause = clauses.back();

  shared_dfa_ptr dfa_a = last_clause.back();
  last_clause.pop_back();

  shared_dfa_ptr dfa_b = last_clause.back();
  last_clause.pop_back();

  last_clause.emplace_back(DFAUtil<ndim, shape_pack...>::get_intersection(dfa_a, dfa_b));
  count_manager.inc("get_intersection");

  if(num_clauses > 1)
    {
      assert(clauses[num_clauses - 2].size() <= clauses[num_clauses - 1].size());
    }
}

template <int ndim, int... shape_pack>
void DNFBuilder<ndim, shape_pack...>::compact_last_two()
{
  Profile profile("compact_last_two");

  assert(clauses.size() >= 2);

  clause_type& second_last = clauses[clauses.size() - 2];
  clause_type& last_clause = clauses.back();
  assert(second_last.size() == last_clause.size());

  // union last DFA of the last two clauses
  shared_dfa_ptr dfa_a = second_last.back();
  shared_dfa_ptr dfa_b = last_clause.back();
  second_last.back() = DFAUtil<ndim, shape_pack...>::get_union(dfa_a, dfa_b);
  count_manager.inc("get_union");

  // drop last clause
  clauses.pop_back();
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
