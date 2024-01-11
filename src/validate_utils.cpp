// validate_utils.cpp

#include "validate_utils.h"

#include <iostream>

#include "DFAUtil.h"

template <class... Args>
shared_dfa_ptr load_helper(const Game& game, const std::format_string<Args...>& name_format, Args&&... args)
{
  std::string name = std::format(name_format, std::forward<Args>(args)...);
  std::cout << "LOADING " << name << std::endl;

  shared_dfa_ptr output = game.load_by_name(name);
  if(!output)
    {
      std::cerr << "LOADING FAILED FOR " << name << std::endl;
    }
  return output;
}

template shared_dfa_ptr load_helper(const Game& game, const std::format_string<int&, int&, int&>&, int&, int&, int&);
template shared_dfa_ptr load_helper(const Game& game, const std::format_string<int&, int&, std::string&>&, int&, int&, std::string&);
template shared_dfa_ptr load_helper(const Game& game, const std::format_string<int&>&, int&);

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

bool validate_losing(const Game& game, int side_to_move, shared_dfa_ptr curr_losing, shared_dfa_ptr next_winning, shared_dfa_ptr base_losing, int max_examples)
{
  std::cout << " CHECKING EXAMPLES" << std::endl;

  int losing_examples = 0;
  for(auto iter = curr_losing->cbegin();
	  (iter < curr_losing->cend()) && (losing_examples < max_examples);
	  ++iter, ++losing_examples)
    {
      DFAString position(*iter);

      int result = game.validate_result(side_to_move, position);
      if(result > 0)
	{
	  std::cerr << "LOSING EXAMPLE ACTUALLY WON" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      if(base_losing->contains(position))
	{
	  continue;
	}
      else if(result < 0)
	{
	  std::cerr << "LOST EXAMPLE MISSED BY BASE CASE" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
      if(moves.size() == 0)
	{
	  std::cerr << "NOT LOST EXAMPLE WITHOUT MOVES" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      for(const DFAString& move : moves)
	{
	  if(!next_winning->contains(move))
	    {
	      std::cerr << "LOSING EXAMPLE WITHOUT FORCED LOSS" << std::endl;
	      std::cerr << game.position_to_string(position) << std::endl;
	      return false;
	    }
	}
    }

  std::cout << "checked " << losing_examples << " losing examples." << std::endl;

  return true;
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

bool validate_winning(const Game& game, int side_to_move, shared_dfa_ptr curr_winning, shared_dfa_ptr next_losing, shared_dfa_ptr base_winning, int max_examples)
{
  std::cout << " CHECK EXAMPLES" << std::endl;

  int winning_examples = 0;
  for(auto iter = curr_winning->cbegin();
      (iter < curr_winning->cend()) && (winning_examples < max_examples);
      ++iter, ++winning_examples)
    {
      DFAString position(*iter);

      int result = game.validate_result(side_to_move, position);
      if(result < 0)
	{
	  std::cerr << "WINNING EXAMPLE ACTUALLY LOST" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      if(base_winning->contains(position))
	{
	  continue;
	}
      else if(result > 0)
	{
	  std::cerr << "WON EXAMPLE MISSED BY BASE CASE" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
      if(moves.size() == 0)
	{
	  std::cerr << "NOT WON EXAMPLE WITHOUT MOVES" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      bool found_win = false;
      for(const DFAString& move : moves)
	{
	  if(next_losing->contains(move))
	    {
	      found_win = true;
	      break;
	    }
	}

      if(!found_win)
	{
	  std::cerr << "WINNING EXAMPLE WITHOUT WINNING MOVE" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}
    }

  std::cout << "checked " << winning_examples << " winning examples." << std::endl;

  return true;
}
