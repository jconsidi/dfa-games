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

bool validate_result(const Game& game, int side_to_move, shared_dfa_ptr positions, int result_expected, int max_examples)
{
  if(result_expected != 0)
    {
      // verify these are terminal positions according to the DFA move
      // generator.

      std::cout << " CHECK NO MOVES (bulk)" << std::endl;

      shared_dfa_ptr moves = game.get_moves_forward(side_to_move, positions);
      if(!moves->is_constant(0))
	{
	  double moves_size = moves->size();
	  std::cerr << "POSITIONS NOT TERMINAL: found " << moves_size << " moves for positions with expected result " << result_expected << std::endl;
	  return false;
	}
    }

  std::cout << " CHECK EXAMPLES" << std::endl;

  int result_examples = 0;
  for(auto iter = positions->cbegin();
      (iter < positions->cend()) && (result_examples < max_examples);
      ++iter, ++result_examples)
    {
      DFAString position(*iter);

      int result_actual = game.validate_result(side_to_move, position);
      bool result_mismatch = result_actual != result_expected;

      std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
      bool moves_mismatch = (result_expected != 0) && (moves.size() != 0);

      if(result_mismatch || moves_mismatch)
	{
	  std::cerr << game.position_to_string(position) << std::endl;

	  if(result_mismatch)
	    {
	      std::cerr << "RESULT MISMATCH: expected " << result_expected << ", actual " << result_actual << std::endl;
	    }

	  if(moves_mismatch)
	    {
	      std::cerr << "MOVES MISMATCH: expected " << 0 << ", actual " << moves.size() << std::endl;
	    }

	  std::cerr << std::endl;

	  return false;
	}
    }

  std::cout << "checked " << result_examples << " examples" << std::endl;

  return true;
}

bool validate_subset(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b)
{
  shared_dfa_ptr subset_check = DFAUtil::get_difference(dfa_a, dfa_b);
  return subset_check->is_constant(0);
}
