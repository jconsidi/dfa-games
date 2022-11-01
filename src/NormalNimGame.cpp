// NormalNimGame.cpp

#include "NormalNimGame.h"

#include <vector>

#include "DFAUtil.h"

template<int n, int... shape_pack>
NormalNimGame<n, shape_pack...>::NormalNimGame()
  : NormalPlayGame<n, shape_pack...>("normalnim" + std::to_string(n))
{
}

template<int n, int... shape_pack>
typename NormalNimGame<n, shape_pack...>::step_vector NormalNimGame<n, shape_pack...>::get_steps_internal(int) const
{
  step_vector steps;
  steps.emplace_back();

  rule_vector& rules = steps[0];

  // no conditions needed
  const std::vector<shared_dfa_ptr> conditions;

  shared_dfa_ptr dummy = DFAUtil<n, shape_pack...>::get_reject();
  for(int layer = 0; layer < n; ++layer)
    {
      int layer_shape = dummy->get_layer_shape(layer);
      for(int before = 1; before < layer_shape; ++before)
	{
	  for(int after = 0; after < before; ++after)
	    {
	      change_vector changes(n);
	      changes[layer] = change_type(before, after);
	      rules.emplace_back(conditions,
				 changes,
				 conditions,
				 std::to_string(layer) + ": " + std::to_string(before) + "->" + std::to_string(after));
	    }
	}
    }

  return steps;
}

template<int n, int... shape_pack>
typename NormalNimGame<n, shape_pack...>::shared_dfa_ptr NormalNimGame<n, shape_pack...>::get_positions_initial() const
{
  shared_dfa_ptr dummy = DFAUtil<n, shape_pack...>::get_reject();

  std::vector<int> initial_string;
  for(int layer = 0; layer < n; ++layer)
    {
      initial_string.push_back(dummy->get_layer_size(layer));
    }

  return DFAUtil<n, shape_pack...>::from_string(dfa_string_type(initial_string));
}

template<int n, int... shape_pack>
std::string NormalNimGame<n, shape_pack...>::position_to_string(const typename NormalNimGame<n, shape_pack...>::dfa_string_type& string_in) const
{
  std::string output;
  output += std::to_string(string_in[0]);
  for(int layer = 1; layer < n; ++layer)
    {
      output += "," + std::to_string(string_in[layer]);
    }
  return output;
}

// template instantiations

template class NormalNimGame<1, NORMALNIM1_SHAPE_PACK>;
template class NormalNimGame<2, NORMALNIM2_SHAPE_PACK>;
template class NormalNimGame<3, NORMALNIM3_SHAPE_PACK>;
template class NormalNimGame<4, NORMALNIM4_SHAPE_PACK>;