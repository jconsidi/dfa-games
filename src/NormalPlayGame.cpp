// NormalPlayGame.cpp

#include "NormalPlayGame.h"

#include <sstream>

#include "DFAUtil.h"

template <int ndim, int... shape_pack>
NormalPlayGame<ndim, shape_pack...>::NormalPlayGame(std::string name_in)
  : Game<ndim, shape_pack...>(name_in)
{
}

template <int ndim, int... shape_pack>
typename NormalPlayGame<ndim, shape_pack...>::shared_dfa_ptr NormalPlayGame<ndim, shape_pack...>::get_positions_losing(int side_to_move,
														       int ply_max) const
{
  assert(ply_max >= 0);

  if(ply_max % 2)
    {
      // losses are always in an even number of ply
      --ply_max;
    }

  std::ostringstream dfa_name_builder;
  dfa_name_builder << "losing,side=" << side_to_move << ",ply_max=" << ply_max;

  return this->load_or_build(dfa_name_builder.str(),
			     [=]()
			     {
			       shared_dfa_ptr winning_soon =
				 ((ply_max <= 0) ?
				  DFAUtil<ndim, shape_pack...>::get_reject() :
				  get_positions_winning(1 - side_to_move, ply_max - 1));

			       shared_dfa_ptr not_winning_soon = DFAUtil<ndim, shape_pack...>::get_inverse(winning_soon);
			       shared_dfa_ptr not_losing_soon = this->get_moves_backward(side_to_move, not_winning_soon);
			       shared_dfa_ptr losing_soon = DFAUtil<ndim, shape_pack...>::get_inverse(not_losing_soon);
			       return losing_soon;
			     });
}


template <int ndim, int... shape_pack>
typename NormalPlayGame<ndim, shape_pack...>::shared_dfa_ptr NormalPlayGame<ndim, shape_pack...>::get_positions_lost(int side_to_move) const
{
  return get_positions_losing(side_to_move, 0);
}

template <int ndim, int... shape_pack>
typename NormalPlayGame<ndim, shape_pack...>::shared_dfa_ptr NormalPlayGame<ndim, shape_pack...>::get_positions_winning(int side_to_move,
															int ply_max) const
{
  assert(ply_max >= 0);

  if(ply_max <= 0)
    {
      return DFAUtil<ndim, shape_pack...>::get_reject();
    }

  if(ply_max % 2 == 0)
    {
      // wins are always in an odd number of ply
      --ply_max;
    }

  std::ostringstream dfa_name_builder;
  dfa_name_builder << "winning,side=" << side_to_move << ",ply_max=" << ply_max;

  return this->load_or_build(dfa_name_builder.str(),
			     [=]()
			     {
			       shared_dfa_ptr losing_soon = get_positions_losing(1 - side_to_move, ply_max - 1);
			       shared_dfa_ptr winning_soon = this->get_moves_backward(side_to_move, losing_soon);
			       return winning_soon;
			     });
}

template <int ndim, int... shape_pack>
typename NormalPlayGame<ndim, shape_pack...>::shared_dfa_ptr NormalPlayGame<ndim, shape_pack...>::get_positions_won(int side_to_move) const
{
  return get_positions_winning(side_to_move, 0);
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(NormalPlayGame);
