// FixedDFA.cpp

#include "FixedDFA.h"

FixedDFA::FixedDFA(const dfa_shape_t& shape_in, int fixed_square, int fixed_character)
  : DedupedDFA(shape_in)
{
  // 1 state until the fixed square, then a reject state and accept
  // state until the penultimate (mask) layer.

  int ndim = get_shape_size();
  assert((0 <= fixed_square) && (fixed_square < ndim));

  // fixed layer

  int fixed_state_id = this->add_state_by_function(fixed_square, [fixed_character](int i){return (i == fixed_character);});

  // layers before fixed square (if any)

  int next_state_id = fixed_state_id;
  for(int layer = fixed_square - 1; layer >= 0; --layer)
    {
      next_state_id = this->add_state_by_function(layer, [=](int){return next_state_id;});
    }

  // done
  this->set_initial_state(next_state_id);
}
