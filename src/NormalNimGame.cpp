// NormalNimGame.cpp

#include "NormalNimGame.h"

#include <vector>

#include "DFAUtil.h"

NormalNimGame::NormalNimGame(const dfa_shape_t& shape_in)
  : NormalPlayGame("normalnim" + std::to_string(shape_in.size()), shape_in)
{
}

Game::phase_vector NormalNimGame::get_phases_internal(int) const
{
  int n = get_shape().size();

  phase_vector phases;
  phases.emplace_back();

  choice_vector& choices = phases[0];

  // no conditions needed
  const std::vector<shared_dfa_ptr> conditions;

  shared_dfa_ptr dummy = DFAUtil::get_reject(get_shape());
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

shared_dfa_ptr NormalNimGame::get_positions_initial() const
{
  shared_dfa_ptr dummy = DFAUtil::get_reject(get_shape());

  std::vector<int> initial_string;
  for(int layer = 0; layer < get_shape().size(); ++layer)
    {
      initial_string.push_back(dummy->get_layer_size(layer));
    }

  return DFAUtil::from_string(DFAString(get_shape(), initial_string));
}

std::string NormalNimGame::position_to_string(const DFAString& string_in) const
{
  std::string output;
  output += std::to_string(string_in[0]);
  for(int layer = 1; layer < get_shape().size(); ++layer)
    {
      output += "," + std::to_string(string_in[layer]);
    }
  return output;
}
