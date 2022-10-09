// DFAUtil.cpp

#include "DFAUtil.h"

#include "AcceptDFA.h"
#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
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
  static shared_dfa_ptr accept(new AcceptDFA<ndim, shape_pack...>());
  assert(accept);
  return accept;
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_fixed(int fixed_square, int fixed_character)
{
  // TODO: cache this?
  return shared_dfa_ptr(new FixedDFA<ndim, shape_pack...>(fixed_square, fixed_character));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_intersection(DFAUtil<ndim, shape_pack...>::shared_dfa_ptr left_in, DFAUtil<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  std::optional<bool> left_constant = check_constant(left_in);
  if(left_constant.has_value())
    {
      bool left_value = *left_constant;
      if(left_value)
	{
	  return right_in;
	}
      else
	{
	  return get_reject();
	}
    }

  std::optional<bool> right_constant = check_constant(right_in);
  if(right_constant.has_value())
    {
      bool right_value = *right_constant;
      if(right_value)
	{
	  return left_in;
	}
      else
	{
	  return get_reject();
	}
    }

  return shared_dfa_ptr(new IntersectionDFA<ndim, shape_pack...>(*left_in, *right_in));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_intersection(const std::vector<DFAUtil<ndim, shape_pack...>::shared_dfa_ptr>& dfas_in)
{
  // TODO : add short-circuit filtering
  return shared_dfa_ptr(new IntersectionDFA<ndim, shape_pack...>(dfas_in));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_inverse(DFAUtil<ndim, shape_pack...>::shared_dfa_ptr dfa_in)
{
  // TODO : short-circuit logic for constants?
  return shared_dfa_ptr(new InverseDFA<ndim, shape_pack...>(*dfa_in));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_reject()
{
  static shared_dfa_ptr reject(new RejectDFA<ndim, shape_pack...>());
  assert(reject);
  return reject;
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_union(DFAUtil<ndim, shape_pack...>::shared_dfa_ptr left_in, DFAUtil<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  std::optional<bool> left_constant = check_constant(left_in);
  if(left_constant.has_value())
    {
      bool left_value = *left_constant;
      if(left_value)
	{
	  return get_accept();
	}
      else
	{
	  return right_in;
	}
    }

  std::optional<bool> right_constant = check_constant(right_in);
  if(right_constant.has_value())
    {
      bool right_value = *right_constant;
      if(right_value)
	{
	  return get_accept();
	}
      else
	{
	  return left_in;
	}
    }

  return shared_dfa_ptr(new UnionDFA<ndim, shape_pack...>(*left_in, *right_in));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_union(const std::vector<DFAUtil<ndim, shape_pack...>::shared_dfa_ptr>& dfas_in)
{
  // TODO : add short-circuit filtering
  return shared_dfa_ptr(new UnionDFA<ndim, shape_pack...>(dfas_in));
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(DFAUtil);
