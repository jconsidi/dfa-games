// IntersectionManager.cpp

#include "IntersectionManager.h"

#include "CountManager.h"
#include "DFAUtil.h"

static CountManager count_manager("IntersectionManager");

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
  count_manager.inc("get_intersection");
  return output;
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(IntersectionManager);
