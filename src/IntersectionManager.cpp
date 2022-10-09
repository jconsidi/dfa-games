// IntersectionManager.cpp

#include "IntersectionManager.h"

#include "DFAUtil.h"

template<int ndim, int... shape_pack>
IntersectionManager<ndim, shape_pack...>::IntersectionManager()
  : intersect_cache()
{
}

template<int ndim, int... shape_pack>
typename IntersectionManager<ndim, shape_pack...>::shared_dfa_ptr IntersectionManager<ndim, shape_pack...>::intersect(typename IntersectionManager<ndim, shape_pack...>::shared_dfa_ptr left_in, typename IntersectionManager<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  std::pair<shared_dfa_ptr,shared_dfa_ptr> key(left_in, right_in);
  auto search = intersect_cache.find(key);
  if(search != intersect_cache.end())
    {
      return search->second;
    }

  shared_dfa_ptr output = DFAUtil<ndim, shape_pack...>::get_intersection(left_in, right_in);
  intersect_cache[key] = output;
  return output;
}

// template instantiations

#include "ChessDFAParams.h"
#include "TicTacToeDFAParams.h"

template class IntersectionManager<CHESS_DFA_PARAMS>;
template class IntersectionManager<TICTACTOE2_DFA_PARAMS>;
template class IntersectionManager<TICTACTOE3_DFA_PARAMS>;
template class IntersectionManager<TICTACTOE4_DFA_PARAMS>;
