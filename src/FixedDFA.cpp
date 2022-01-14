// FixedDFA.cpp

#include "FixedDFA.h"

template<int ndim, int... shape_pack>
FixedDFA<ndim, shape_pack...>::FixedDFA(int fixed_square, int fixed_character)
  : DFA<ndim,shape_pack...>()
{
  // 1 state until the fixed square, then a reject state and accept
  // state until the penultimate (mask) layer.

  assert((0 <= fixed_square) && (fixed_square < ndim));

  // layers after fixed square

  for(int layer = ndim - 1; layer > fixed_square; --layer)
    {
      // already decided accept/reject in previous square and just propagating

      for(int state = 0; state < 2; ++state)
	{
	  // already decided accept/reject in previous square and just propagating

	  // reject case
	  int reject_state_id = this->add_state(layer, [](int i){return 0;});
	  assert(reject_state_id == 0);

	  // accept case
	  int accept_state_id = this->add_state(layer, [](int i){return 1;});
	  assert(accept_state_id == 1);
	}
    }

  // fixed layer

  int fixed_state_id = this->add_state(fixed_square, [fixed_character](int i){return (i == fixed_character);});
  assert(fixed_state_id == 0);

  // layers before fixed square (if any)

  for(int layer = fixed_square - 1; layer >= 0; --layer)
    {
      int early_state_id = this->add_state(layer, [](int i){return 0;});
      assert(early_state_id == 0);
    }
}

// template instantiations

#include "ChessDFA.h"
#include "TicTacToeGame.h"

template class FixedDFA<CHESS_TEMPLATE_PARAMS>;
template class FixedDFA<TICTACTOE2_DFA_PARAMS>;
template class FixedDFA<TICTACTOE3_DFA_PARAMS>;
template class FixedDFA<TICTACTOE4_DFA_PARAMS>;
