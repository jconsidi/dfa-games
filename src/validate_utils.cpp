// validate_utils.cpp

#include "validate_utils.h"

#include <iostream>

#include "DFAUtil.h"

bool validate_disjoint(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b)
{
  return DFAUtil::get_intersection(dfa_a, dfa_b)->is_constant(0);
}

bool validate_equal(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b)
{
  if(dfa_a->get_shape() != dfa_b->get_shape())
    {
      throw std::logic_error("shape mismatch");
    }

  if(dfa_a->get_hash() == dfa_b->get_hash())
    {
      // trust hash matches
      return true;
    }

  // hashes may be different if states are in a different order...

  return (DFAUtil::get_difference(dfa_a, dfa_b)->is_constant(false) &&
	  DFAUtil::get_difference(dfa_b, dfa_a)->is_constant(false));
}

bool validate_partition(shared_dfa_ptr target, std::vector<shared_dfa_ptr> partition)
{
  shared_dfa_ptr union_check = DFAUtil::get_union_vector(target->get_shape(), partition);
  if(!validate_equal(union_check, target))
    {
      std::cerr << "PARTITION CHECK FAILED: UNION MISMATCH" << std::endl;
      return false;
    }

  for(int i = 0; i < partition.size(); ++i)
    {
      for(int j = i + 1; j < partition.size(); ++j)
	{
	  if(!validate_disjoint(partition[i], partition[j]))
	    {
	      std::cerr << "PARTITION CHECK FAILED: PARTITIONS " << i << "+" << j << " ARE NOT DISJOINT" << std::endl;
	      return false;
	    }
	}
    }

  return true;
}

bool validate_subset(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b)
{
  shared_dfa_ptr subset_check = DFAUtil::get_difference(dfa_a, dfa_b);
  return subset_check->is_constant(0);
}
