// NormalNimGame.cpp

#include "NormalNimGame.h"

#include <format>
#include <vector>

#include "DFAUtil.h"

NormalNimGame::NormalNimGame(int num_heaps, int heap_max)
  : NormalPlayGame(std::format("normalnim_{:d}x{:d}", num_heaps, heap_max),
		   dfa_shape_t(num_heaps, heap_max + 1))
{
}

MoveGraph NormalNimGame::build_move_graph(int) const
{
  int n = get_shape().size();

  MoveGraph move_graph(get_shape());

  // setup nodes

  move_graph.add_node("begin");

  std::vector<std::string> change_names;
  for(int layer = 0; layer < n; ++layer)
    {
      int layer_shape = get_shape()[layer];
      for(int before = 1; before < layer_shape; ++before)
	{
	  for(int after = 0; after < before; ++after)
	    {
	      change_vector changes(n);
	      changes[layer] = change_type(before, after);

	      std::string change_name = std::to_string(layer) + ": " + std::to_string(before) + "->" + std::to_string(after);
	      move_graph.add_node(change_name,
				  changes);
	      change_names.push_back(change_name);
	    }
	}
    }

  move_graph.add_node("end");

  // setup edges

  for(std::string change_name : change_names)
    {
      move_graph.add_edge("begin to " + change_name,
			  "begin",
			  change_name);
      move_graph.add_edge(change_name + " to end",
			  change_name,
			  "end");
    }

  return move_graph;
}

DFAString NormalNimGame::get_position_initial() const
{
  std::vector<int> initial_string;
  for(int layer = 0; layer < get_shape().size(); ++layer)
    {
      initial_string.push_back(get_shape()[layer] - 1);
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

std::vector<DFAString> NormalNimGame::validate_moves(int, DFAString) const
{
  throw std::logic_error("not implemented");
}
