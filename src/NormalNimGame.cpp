// NormalNimGame.cpp

#include "NormalNimGame.h"

#include <vector>

#include "DFAUtil.h"

NormalNimGame::NormalNimGame(const dfa_shape_t& shape_in)
  : NormalPlayGame("normalnim_" + std::to_string(shape_in.size()), shape_in)
{
}

MoveGraph NormalNimGame::build_move_graph(int) const
{
  int n = get_shape().size();

  MoveGraph move_graph;
  move_graph.add_node("begin");
  move_graph.add_node("end");

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
	      move_graph.add_edge(std::to_string(layer) + ": " + std::to_string(before) + "->" + std::to_string(after),
				  "begin",
				  "end",
				  conditions,
				  changes,
				  conditions);
	    }
	}
    }

  return move_graph;
}

DFAString NormalNimGame::get_position_initial() const
{
  shared_dfa_ptr dummy = DFAUtil::get_reject(get_shape());

  std::vector<int> initial_string;
  for(int layer = 0; layer < get_shape().size(); ++layer)
    {
      initial_string.push_back(dummy->get_layer_size(layer));
    }

  return DFAString(get_shape(), initial_string);
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
