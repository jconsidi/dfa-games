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

