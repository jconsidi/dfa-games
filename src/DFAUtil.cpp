// DFAUtil.cpp

#include "DFAUtil.h"

#include "AcceptDFA.h"
#include "IntersectionDFA.h"
#include "RejectDFA.h"
#include "UnionDFA.h"

template<int ndim, int... shape_pack>
std::optional<bool> DFAUtil<ndim, shape_pack...>::check_constant(typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr dfa_in)
{
  int initial_state = dfa_in->get_initial_state();
  if(initial_state < 2)
    {
      return std::optional<bool>(bool(initial_state));
    }
  else
    {
      // found a difference
      return std::optional<bool>();
    }
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_accept()
{
  return shared_dfa_ptr(new AcceptDFA<ndim, shape_pack...>());
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_intersection(DFAUtil<ndim, shape_pack...>::shared_dfa_ptr left_in, DFAUtil<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  return shared_dfa_ptr(new IntersectionDFA<ndim, shape_pack...>(*left_in, *right_in));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_reject()
{
  return shared_dfa_ptr(new RejectDFA<ndim, shape_pack...>());
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_union(DFAUtil<ndim, shape_pack...>::shared_dfa_ptr left_in, DFAUtil<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  return shared_dfa_ptr(new UnionDFA<ndim, shape_pack...>(*left_in, *right_in));
}



// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(DFAUtil);
