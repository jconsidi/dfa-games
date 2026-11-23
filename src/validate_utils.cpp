// validate_utils.cpp

#include "validate_utils.h"

#include <iostream>

#include "DFAUtil.h"

static void print_example(const Game& game, shared_dfa_ptr positions)
{
  for(auto iter = positions->cbegin();
          iter < positions->cend();
      ++iter)
    {
      std::cerr << game.position_to_string(*iter) << std::endl;
      break;
    }
}

template <class... Args>
shared_dfa_ptr load_helper(const Game& game, const std::format_string<Args...>& name_format, Args&&... args)
{
  std::string name = std::format(name_format, std::forward<Args>(args)...);
  std::cout << "# LOADING " << name << std::endl;

  shared_dfa_ptr output = game.load_by_name(name);
  if(!output)
    {
      std::cerr << "# LOADING FAILED FOR " << name << std::endl;
    }
  return output;
}

template shared_dfa_ptr load_helper(const Game& game, const std::format_string<int&, int&, int&>&, int&, int&, int&);
template shared_dfa_ptr load_helper(const Game& game, const std::format_string<int&, int&, std::string&>&, int&, int&, std::string&);
template shared_dfa_ptr load_helper(const Game& game, const std::format_string<int&>&, int&);

bool validate_equal(const Game& game, std::string name_a, shared_dfa_ptr dfa_a, std::string name_b, shared_dfa_ptr dfa_b)
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

  shared_dfa_ptr dfa_a_minus_b = DFAUtil::get_difference(dfa_a, dfa_b);
  if(!dfa_a_minus_b->is_constant(false))
    {
      auto size_a_minus_b = dfa_a_minus_b->size();
      std::cerr << "# " << name_a << " HAS " << size_a_minus_b << " EXTRA POSITIONS" << std::endl;
      print_example(game, dfa_a_minus_b);
    }

  shared_dfa_ptr dfa_b_minus_a = DFAUtil::get_difference(dfa_b, dfa_a);
  if(!dfa_b_minus_a->is_constant(false))
    {
      auto size_b_minus_a = dfa_b_minus_a->size();
      std::cerr << "# " << name_b << " HAS " << size_b_minus_a << " EXTRA POSITIONS" << std::endl;
      print_example(game, dfa_b_minus_a);
    }

  return (dfa_a_minus_b->is_constant(false) &&
          dfa_b_minus_a->is_constant(false));

  return true;
}

bool validate_losing(const Game& game, int side_to_move, shared_dfa_ptr curr_losing, shared_dfa_ptr next_winning, shared_dfa_ptr base_losing, int max_examples)
{
  std::cout << "# CHECKING EXAMPLES" << std::endl;

  int losing_examples = 0;
  for(auto iter = curr_losing->cbegin();
	  (iter < curr_losing->cend()) && (losing_examples < max_examples);
	  ++iter, ++losing_examples)
    {
      DFAString position(*iter);

      if(base_losing->contains(position))
        {
          // previously verified as losing
          continue;
        }

      ValidatedPosition validated = game.validate_position(side_to_move, position);
      const GameOver *over = std::get_if<GameOver>(&validated);
      std::optional<int> result = over ? std::optional<int>(over->result) : std::optional<int>();
      if(!result)
        {
          std::cerr << "# LOSING EXAMPLE WITHOUT RESULT" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}
      if(*result > 0)
	{
	  std::cerr << "# LOSING EXAMPLE ACTUALLY WON" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}
      if(*result < 0)
	{
	  std::cerr << "LOST EXAMPLE MISSED BY BASE CASE" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      std::vector<DFAString> moves = validated_moves(validated);
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
	      std::cerr << "# LOSING EXAMPLE WITHOUT FORCED LOSS" << std::endl;
	      std::cerr << game.position_to_string(position) << std::endl;
	      return false;
	    }
	}
    }

  std::cout << "# checked " << losing_examples << " losing examples." << std::endl;

  return true;
}

bool validate_partition(shared_dfa_ptr target, std::vector<shared_dfa_ptr> partition, int max_examples)
{
  size_t partition_examples = 0;
  for(auto iter = target->cbegin();
      (iter < target->cend()) && (partition_examples < max_examples);
      ++iter, ++partition_examples)
    {
      DFAString position(*iter);

      int partition_matches = 0;
      for(const shared_dfa_ptr& p_s : partition)
        {
          if(p_s->contains(position))
            {
              ++partition_matches;
            }
        }

      if(partition_matches == 0)
        {
          std::cerr << "# PARTITION CHECK FAILED: FOUND POSITION NOT IN ANY PARTITION" << std::endl;
          return false;
        }
      if(partition_matches != 1)
        {
          std::cerr << "# PARTITION CHECK FAILED: PARTITIONS NOT DISJOINT" << std::endl;
          return false;
        }
    }

  for(const shared_dfa_ptr& p_s : partition)
    {
      if(!validate_subset(p_s, target, max_examples))
        {
          std::cerr << "# PARTITION CHECKED FAILED: SUBSET CHECK FAILED" << std::endl;
          return false;
        }
    }

  return true;
}

bool validate_subset(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b, int max_examples)
{
  int subset_examples = 0;
  for(auto iter = dfa_a->cbegin();
      (iter < dfa_a->cend()) && (subset_examples < max_examples);
      ++iter, ++subset_examples)
    {
      DFAString position(*iter);

      if(!dfa_b->contains(position))
        {
          return false;
        }
    }

  return true;
}

bool validate_winning(const Game& game, int side_to_move, shared_dfa_ptr curr_winning, shared_dfa_ptr next_losing, shared_dfa_ptr base_winning, int max_examples)
{
  std::cout << "#  CHECK EXAMPLES" << std::endl;

  int winning_examples = 0;
  for(auto iter = curr_winning->cbegin();
      (iter < curr_winning->cend()) && (winning_examples < max_examples);
      ++iter, ++winning_examples)
    {
      DFAString position(*iter);

      if(base_winning->contains(position))
        {
          // previously verified as winning
          continue;
        }

      ValidatedPosition validated = game.validate_position(side_to_move, position);
      const GameOver *over = std::get_if<GameOver>(&validated);
      std::optional<int> result = over ? std::optional<int>(over->result) : std::optional<int>();
      if(!result)
        {
          std::cerr << "# WINNING EXAMPLE WITHOUT RESULT" << std::endl;
          std::cerr << game.position_to_string(position) << std::endl;
          return false;
        }
      if(*result < 0)
	{
	  std::cerr << "# WINNING EXAMPLE ACTUALLY LOST" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}
      if(*result > 0)
	{
	  std::cerr << "# WON EXAMPLE MISSED BY BASE CASE" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}

      std::vector<DFAString> moves = validated_moves(validated);
      if(moves.size() == 0)
	{
	  std::cerr << "# NOT WON EXAMPLE WITHOUT MOVES" << std::endl;
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
	  std::cerr << "# WINNING EXAMPLE WITHOUT WINNING MOVE" << std::endl;
	  std::cerr << game.position_to_string(position) << std::endl;
	  return false;
	}
    }

  std::cout << "# checked " << winning_examples << " winning examples." << std::endl;

  return true;
}
