// DFAUtil.cpp

#include "DFAUtil.h"

#include <map>

#include "AcceptDFA.h"
#include "DifferenceDFA.h"
#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "RejectDFA.h"
#include "StringDFA.h"
#include "UnionDFA.h"

template<int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::_singleton_if_constant(typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr dfa_in)
{
  dfa_state_t initial_state = dfa_in->get_initial_state();
  if(initial_state == 0)
    {
      return get_reject();
    }
  else if(initial_state == 1)
    {
      return get_accept();
    }

  return dfa_in;
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::from_strings(const std::vector<dfa_string_type>& strings_in)
{
  // degenerate case

  if(strings_in.size() <= 0)
    {
      return get_reject();
    }

  // build new DFA

  return shared_dfa_ptr(new StringDFA<ndim, shape_pack...>(strings_in));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_accept()
{
  static shared_dfa_ptr accept(new AcceptDFA<ndim, shape_pack...>());
  assert(accept);
  return accept;
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_difference(DFAUtil<ndim, shape_pack...>::shared_dfa_ptr left_in, DFAUtil<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  if(left_in->is_constant(true))
    {
      return get_inverse(right_in);
    }
  else if(left_in->is_constant(false))
    {
      return get_reject();
    }

  if(right_in->is_constant(true))
    {
      return get_reject();
    }
  else if(right_in->is_constant(false))
    {
      return left_in;
    }

  return _singleton_if_constant(shared_dfa_ptr(new DifferenceDFA<ndim, shape_pack...>(*left_in, *right_in)));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_fixed(int fixed_layer, int fixed_character)
{
  static std::map<std::pair<int, int>, shared_dfa_ptr> singletons;

  std::pair<int, int> singleton_key(fixed_layer, fixed_character);
  auto search = singletons.find(singleton_key);
  if(search != singletons.end())
    {
      return search->second;
    }

  shared_dfa_ptr output = shared_dfa_ptr(new FixedDFA<ndim, shape_pack...>(fixed_layer, fixed_character));
  output->set_name("get_fixed(fixed_layer=" + std::to_string(fixed_layer) + ", fixed_character=" + std::to_string(fixed_character) + ")");
  singletons[singleton_key] = output;
  return output;
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_intersection(DFAUtil<ndim, shape_pack...>::shared_dfa_ptr left_in, DFAUtil<ndim, shape_pack...>::shared_dfa_ptr right_in)
{
  if(left_in->is_constant(true))
    {
      return right_in;
    }
  else if(left_in->is_constant(false))
    {
      return get_reject();
    }

  if(right_in->is_constant(true))
    {
      return left_in;
    }
  else if(right_in->is_constant(false))
    {
      return get_reject();
    }

  return _singleton_if_constant(shared_dfa_ptr(new IntersectionDFA<ndim, shape_pack...>(*left_in, *right_in)));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_intersection_vector(const std::vector<DFAUtil<ndim, shape_pack...>::shared_dfa_ptr>& dfas_in)
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
  if(left_in->is_constant(true))
    {
      return get_accept();
    }
  else if(left_in->is_constant(false))
    {
      return right_in;
    }

  if(right_in->is_constant(true))
    {
      return get_accept();
    }
  else if(right_in->is_constant(false))
    {
      return left_in;
    }

  return _singleton_if_constant(shared_dfa_ptr(new UnionDFA<ndim, shape_pack...>(*left_in, *right_in)));
}

template <int ndim, int... shape_pack>
typename DFAUtil<ndim, shape_pack...>::shared_dfa_ptr DFAUtil<ndim, shape_pack...>::get_union_vector(const std::vector<DFAUtil<ndim, shape_pack...>::shared_dfa_ptr>& dfas_in)
{
  // TODO : add short-circuit filtering
  return shared_dfa_ptr(new UnionDFA<ndim, shape_pack...>(dfas_in));
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(DFAUtil);
