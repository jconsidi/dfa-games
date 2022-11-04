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
typename NormalNimGame<n, shape_pack...>::phase_vector NormalNimGame<n, shape_pack...>::get_phases_internal(int) const
{
  phase_vector phases;
  phases.emplace_back();

  choice_vector& choices = phases[0];

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
	      choices.emplace_back(conditions,
				   changes,
				   conditions,
				   std::to_string(layer) + ": " + std::to_string(before) + "->" + std::to_string(after));
	    }
	}
    }

  return phases;
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

template class NormalNimGame<NORMALNIM1_DFA_PARAMS>;
template class NormalNimGame<NORMALNIM2_DFA_PARAMS>;
template class NormalNimGame<NORMALNIM3_DFA_PARAMS>;
template class NormalNimGame<NORMALNIM4_DFA_PARAMS>;
