// IntersectionManager.cpp

#include "IntersectionManager.h"

#include "DFAUtil.h"
#include "RejectDFA.h"

template<int ndim, int... shape_pack>
IntersectionManager<ndim, shape_pack...>::IntersectionManager()
  : intersect_cache(),
    reject_all(new RejectDFA<ndim, shape_pack...>())
{
}

template<int ndim, int... shape_pack>
typename IntersectionManager<ndim, shape_pack...>::shared_dfa_ptr IntersectionManager<ndim, shape_pack...>::intersect(typename IntersectionManager<ndim, shape_pack...>::shared_dfa_ptr left_in, typename IntersectionManager<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  std::optional<bool> left_constant = DFAUtil<ndim, shape_pack...>::check_constant(left_in);
  if(left_constant.has_value())
    {
      bool left_value = *left_constant;
      if(left_value)
	{
	  return right_in;
	}
      else
	{
	  return reject_all;
	}
    }

  std::optional<bool> right_constant = DFAUtil<ndim, shape_pack...>::check_constant(right_in);
  if(right_constant.has_value())
    {
      bool right_value = *right_constant;
      if(right_value)
	{
	  return left_in;
	}
      else
	{
	  return reject_all;
	}
    }

  std::pair<shared_dfa_ptr,shared_dfa_ptr> key(left_in, right_in);
  auto search = intersect_cache.find(key);
  if(search != intersect_cache.end())
    {
      return search->second;
    }

  shared_dfa_ptr output(new intersection_dfa_type(*left_in, *right_in));
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
