// MoveGraph.cpp

#include "MoveGraph.h"

#include <iostream>
#include <memory>
#include <vector>

#include "DFAUtil.h"
#include "DNFBuilder.h"
#include "Profile.h"

static change_vector reverse_changes(const change_vector& changes_in);

MoveGraph::MoveGraph(const dfa_shape_t& shape_in)
  : shape(shape_in),
    node_names(),
    node_names_to_indexes()
{
}

void MoveGraph::add_edge(std::string from_node_name,
			 std::string to_node_name)
{
  add_edge(from_node_name + " to " + to_node_name,
	   from_node_name,
           to_node_name);
}

void MoveGraph::add_edge(std::string edge_name_in,
			 std::string from_node_name,
			 std::string to_node_name)
{
  add_edge(edge_name_in,
	   from_node_name,
	   to_node_name,
	   move_edge_condition_vector(0));
}

void MoveGraph::add_edge(std::string edge_name_in,
			 std::string from_node_name,
			 std::string to_node_name,
			 const move_edge_condition_vector& conditions_in)
{
  if(edge_names.contains(edge_name_in))
    {
      throw std::logic_error("add_edge() duplicate edge name");
    }
  edge_names.insert(edge_name_in);

  int from_node_index = get_node_index(from_node_name);
  int to_node_index = get_node_index(to_node_name);
  assert(from_node_index < to_node_index);

  move_edge_condition_vector conditions;
  auto add_conditions = [&](const move_edge_condition_vector& conditions_source)
  {
    for(shared_dfa_ptr condition : conditions_source)
      {
	assert(condition->get_shape() == shape);
	conditions.push_back(condition);
      }
  };
  add_conditions(node_post_conditions[from_node_index]);
  add_conditions(conditions_in);
  add_conditions(node_pre_conditions[to_node_index]);

  node_edges.at(from_node_index).emplace_back(edge_name_in, conditions, to_node_index);
}

void MoveGraph::add_edge(std::string edge_name_in,
			 std::string from_node_name,
			 std::string to_node_name,
			 shared_dfa_ptr condition_in)
{
  add_edge(edge_name_in,
	   from_node_name,
	   to_node_name,
	   move_edge_condition_vector({condition_in}));
}

void MoveGraph::add_node(std::string node_name_in)
{
  change_vector changes_nop(shape.size());
  add_node(node_name_in, changes_nop);
}

void MoveGraph::add_node(std::string node_name_in, const change_vector& changes_in)
{
  add_node(node_name_in,
	   changes_in,
	   move_edge_condition_vector(),
	   move_edge_condition_vector());
}

void MoveGraph::add_node(std::string node_name_in,
			 const change_vector& changes_in,
			 const move_edge_condition_vector& pre_conditions_in,
			 const move_edge_condition_vector& post_conditions_in)
{
  auto search = node_names_to_indexes.find(node_name_in);
  if(search != node_names_to_indexes.end())
    {
      throw std::logic_error("add_node() duplicate node name");
    }

  assert(changes_in.size() == shape.size());

  node_names_to_indexes[node_name_in] = node_names.size();
  node_names.push_back(node_name_in);

  node_changes.push_back(changes_in);

  node_pre_conditions.push_back(pre_conditions_in);
  node_post_conditions.push_back(post_conditions_in);

  for(int layer = 0; layer < shape.size(); ++layer)
    {
      if(changes_in[layer].has_value())
	{
	  change_type layer_change = *(changes_in[layer]);
	  int before_character = std::get<0>(layer_change);
	  int after_character = std::get<1>(layer_change);

	  node_pre_conditions.back().emplace_back(DFAUtil::get_fixed(shape, layer, before_character));
	  node_post_conditions.back().emplace_back(DFAUtil::get_fixed(shape, layer, after_character));
	}
    }

  node_edges.emplace_back();

  assert(node_names_to_indexes.size() == node_names.size());
  assert(node_changes.size() == node_names.size());
  assert(node_pre_conditions.size() == node_names.size());
  assert(node_post_conditions.size() == node_names.size());
  assert(node_edges.size() == node_names_to_indexes.size());
}

void MoveGraph::add_node(std::string node_name_in, int layer_in, int before_character_in, int after_character_in)
{
  change_vector changes(shape.size());
  changes.at(layer_in) = change_type(before_character_in, after_character_in);

  add_node(node_name_in, changes);
}

shared_dfa_ptr MoveGraph::get_moves(std::string name_prefix, shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves");

  assert(node_names.size() >= 2);

  std::function<shared_dfa_ptr(int)> get_node_output = [&](int node_index)
  {
    std::string output_name = "move_nodes/" + name_prefix + "," + positions_in->get_hash() + ",node=" + std::to_string(node_index);

    return DFAUtil::load_or_build(shape, output_name, [&]()
    {
      profile.tic("node init");

      std::cout << "node " << node_index << "/" << node_names.size() << " (" << node_names[node_index] << ")" << std::endl;

      profile.tic("node inputs");

      DNFBuilder node_builder(positions_in->get_shape());
      if(node_index == 0)
	{
	  node_builder.add_clause(DNFBuilder::clause_type(1, positions_in));
	}
      else
	{
	  // process all edges coming into this node

	  // TODO : restructure node_edges to make this more efficient
	  for(int from_node_index = 0; from_node_index < node_edges.size(); ++from_node_index)
	    {
	      for(int node_edge_index = 0; node_edge_index < node_edges[from_node_index].size(); ++node_edge_index)
		{
		  const move_edge& edge = node_edges[from_node_index][node_edge_index];

		  profile.tic("edge init");

		  const std::string& edge_name = std::get<0>(edge);
		  const move_edge_condition_vector& conditions = std::get<1>(edge);
		  int to_node_index = std::get<2>(edge);
		  if(to_node_index != node_index)
		    {
		      continue;
		    }

		  std::cout << " node " << from_node_index << "/" << node_names.size() << " (" << node_names[from_node_index] << "), edge " << node_edge_index << "/" << node_edges[from_node_index].size() << " (" << edge_name << ")" << std::endl;

		  shared_dfa_ptr edge_positions = get_node_output(from_node_index);

		  // apply choice conditions
		  profile.tic("edge conditions");

		  move_edge_condition_vector conditions_todo = conditions;
		  conditions_todo.push_back(edge_positions);
		  edge_positions = DFAUtil::get_intersection_vector(shape, conditions_todo);

		  std::cout << "  conditions for node " << to_node_index << " (" << node_names[to_node_index] << ") => " << DFAUtil::quick_stats(edge_positions) << std::endl;

		  node_builder.add_clause(DNFBuilder::clause_type(1, edge_positions));
		}
	    }
	}

      // combine all positions coming into this node
      shared_dfa_ptr node_positions_input = node_builder.to_dfa();
      std::cout << " node " << node_index << "/" << node_names.size() << " input: " << DFAUtil::quick_stats(node_positions_input) << std::endl;
      if(node_positions_input->is_constant(0))
	{
	  return node_positions_input;
	}

      profile.tic("node change");

      return DFAUtil::get_change(node_positions_input,
				 node_changes[node_index]);
    });
  };

  return get_node_output(node_names.size() - 1);
}

int MoveGraph::get_node_index(std::string node_name_in) const
{
  auto search = node_names_to_indexes.find(node_name_in);
  if(search == node_names_to_indexes.end())
    {
      throw std::logic_error("get_node_index() node not found");
    }

  return search->second;
}

const move_edge_condition_vector& MoveGraph::get_node_pre_conditions(std::string node_name_in) const
{
  return node_pre_conditions[get_node_index(node_name_in)];
}

MoveGraph MoveGraph::reverse() const
{
  MoveGraph output(shape);

  for(int original_node_index = node_names.size() - 1; original_node_index >= 0; --original_node_index)
    {
      auto reversed_changes = reverse_changes(node_changes[original_node_index]);
      output.add_node(node_names[original_node_index],
		      reversed_changes);
    }

  for(int original_from_node_index = node_names.size() - 1; original_from_node_index >= 0; --original_from_node_index)
    {
      for(const move_edge& original_edge : node_edges[original_from_node_index])
	{
	  const std::string& edge_name = std::get<0>(original_edge);
	  const move_edge_condition_vector& original_conditions = std::get<1>(original_edge);
	  int original_to_node_index = std::get<2>(original_edge);

	  move_edge_condition_vector reverse_conditions;
	  for(auto iter = original_conditions.crbegin();
	      iter < original_conditions.crend();
	      ++iter)
	    {
	      reverse_conditions.push_back(*iter);
	    }
	  assert(reverse_conditions.size() == original_conditions.size());

	  output.add_edge(edge_name,
			  node_names[original_to_node_index],
			  node_names[original_from_node_index],
			  reverse_conditions);
	}
    }

  return output;
}

static change_vector reverse_changes(const change_vector& changes_in)
{
  change_vector changes_out;
  for(int layer = 0; layer < changes_in.size(); ++layer)
    {
      const change_optional& change_in = changes_in[layer];

      if(change_in.has_value())
	{
	  changes_out.emplace_back(change_type(std::get<1>(*change_in),
					       std::get<0>(*change_in)));
	}
      else
	{
	  changes_out.emplace_back();
	}
    }

  return changes_out;
}
