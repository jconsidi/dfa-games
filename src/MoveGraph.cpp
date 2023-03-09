// MoveGraph.cpp

#include "MoveGraph.h"

#include <iostream>
#include <vector>

#include "DFAUtil.h"
#include "DNFBuilder.h"
#include "IntersectionManager.h"
#include "Profile.h"

static change_vector reverse_changes(const change_vector& changes_in);
static void sort_edges(std::vector<move_edge>& edges);

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

shared_dfa_ptr MoveGraph::get_moves(shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves");

  assert(node_names.size() >= 2);

  std::vector<DNFBuilder> node_builders(node_names.size(), positions_in->get_shape());

  node_builders[0].add_clause(DNFBuilder::clause_type(1, positions_in));

  for(int from_node_index = 0; from_node_index < node_names.size(); ++from_node_index)
    {
      profile.tic("node init");

      std::cout << "node " << from_node_index << "/" << node_names.size() << " (" << node_names[from_node_index] << ")" << std::endl;

      profile.tic("node inputs");

      // combine all positions coming into this node
      shared_dfa_ptr from_node_positions_input = node_builders[from_node_index].to_dfa();
      std::cout << " node " << from_node_index << "/" << node_names.size() << " input: " << DFAUtil::quick_stats(from_node_positions_input) << std::endl;
      if(from_node_positions_input->is_constant(0))
	{
	  continue;
	}

      profile.tic("node change");

      shared_dfa_ptr from_node_positions_output =
	DFAUtil::get_change(from_node_positions_input,
			    node_changes[from_node_index]);

      std::cout << " node " << from_node_index << "/" << node_names.size() << " output: " << DFAUtil::quick_stats(from_node_positions_output) << std::endl;
      if(from_node_positions_output->is_constant(0))
	{
	  // not expecting this case since change should have added
	  // pre-conditions to incoming edges.
	  assert(0);
	  continue;
	}

      // sort edges leaving this node for pre-condition efficiency

      sort_edges(node_edges[from_node_index]);

      // process all edges leaving this node

      for(int node_edge_index = 0; node_edge_index < node_edges[from_node_index].size(); ++node_edge_index)
	{
	  profile.tic("edge init");
	  const move_edge& edge = node_edges[from_node_index][node_edge_index];

	  const std::string& edge_name = std::get<0>(edge);
	  const move_edge_condition_vector& conditions = std::get<1>(edge);
	  int to_node_index = std::get<2>(edge);

	  std::cout << " node " << from_node_index << "/" << node_names.size() << " (" << node_names[from_node_index] << "), edge " << node_edge_index << "/" << node_edges[from_node_index].size() << " (" << edge_name << ")" << std::endl;

	  shared_dfa_ptr edge_positions = from_node_positions_output;

	  // apply choice conditions
	  profile.tic("edge conditions");

	  move_edge_condition_vector conditions_todo = conditions;
	  conditions_todo.push_back(edge_positions);
	  edge_positions = DFAUtil::get_intersection_vector(shape, conditions_todo);

	  std::cout << "  conditions for node " << to_node_index << " (" << node_names[to_node_index] << ") => " << DFAUtil::quick_stats(edge_positions) << std::endl;

	  if(edge_positions->is_constant(false))
	    {
	      // no matching positions
	      continue;
	    }

	  // add changed positions and post conditions to output builder

	  profile.tic("add output clause");

	  node_builders[to_node_index].add_clause(std::vector<shared_dfa_ptr>({edge_positions}));
	}

      // clear node input to reclaim space and open files
      node_inputs[from_node_index] = shared_dfa_ptr(0);
    }

  return node_builders.back().to_dfa();
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

static void sort_edges(std::vector<move_edge>& edges)
{
  Profile profile("sort_edges");

  std::stable_sort(edges.begin(), edges.end(), [](const move_edge& a,
						  const move_edge& b)
  {
    // just optimizing pre-condition caching for now to leverage small
    // IntersectionManager cache, and not worrying about DNFBuilder
    // optimization of later nodes.

    // compare pre conditions

    auto pre_a = std::get<1>(a);
    auto pre_b = std::get<1>(b);
    for(int i = 0; (i < pre_a.size()) && (i < pre_b.size()); ++i)
      {
	// sort more states first, so reversing comparisons
	if(pre_a[i]->states() > pre_b[i]->states())
	  {
	    return true;
	  }
	else if(pre_a[i]->states() < pre_b[i]->states())
	  {
	    return false;
	  }

	if(pre_a[i] < pre_b[i])
	  {
	    return true;
	  }
	else if(pre_a[i] > pre_b[i])
	  {
	    return false;
	  }
      }

    if(pre_a.size() < pre_b.size())
      {
	return true;
      }
    else if(pre_b.size() < pre_b.size())
      {
	return false;
      }

    // do not compare change choice
    return false;
  });
}
