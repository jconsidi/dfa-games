// FixedDFA.cpp

#include "FixedDFA.h"

template<int ndim, int... shape_pack>
FixedDFA<ndim, shape_pack...>::FixedDFA(int fixed_square, int fixed_character)
  : DedupedDFA<ndim,shape_pack...>()
{
  // 1 state until the fixed square, then a reject state and accept
  // state until the penultimate (mask) layer.

  assert((0 <= fixed_square) && (fixed_square < ndim));

  // fixed layer

  int fixed_state_id = this->add_state_by_function(fixed_square, [fixed_character](int i){return (i == fixed_character);});

  // layers before fixed square (if any)

  int next_state_id = fixed_state_id;
  for(int layer = fixed_square - 1; layer >= 0; --layer)
    {
      next_state_id = this->add_state_by_function(layer, [=](int i){return next_state_id;});
    }

  // done
  this->set_initial_state(next_state_id);
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(FixedDFA);
