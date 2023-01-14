// MoveGraph.cpp

#include "MoveGraph.h"

#include <iostream>
#include <vector>

#include "DFAUtil.h"
#include "DNFBuilder.h"
#include "IntersectionManager.h"
#include "Profile.h"

static void sort_edges(std::vector<move_edge>& edges);

MoveGraph::MoveGraph()
  : node_names(),
    node_names_to_indexes()
{
}

void MoveGraph::add_edge(std::string edge_name_in,
			 std::string from_node_name,
			 std::string to_node_name,
			 const std::vector<shared_dfa_ptr>& pre_conditions_in,
			 const change_vector& changes_in,
			 const std::vector<shared_dfa_ptr>& post_conditions_in)
{
  if(edge_names.contains(edge_name_in))
    {
      throw std::logic_error("add_edge() duplicate edge name");
    }
  edge_names.insert(edge_name_in);

  int from_node_index = get_node_index(from_node_name);
  int to_node_index = get_node_index(to_node_name);
  assert(from_node_index < to_node_index);

  node_edges.at(from_node_index).emplace_back(edge_name_in, pre_conditions_in, changes_in, post_conditions_in, to_node_index);
}

void MoveGraph::add_node(std::string node_name_in)
{
  auto search = node_names_to_indexes.find(node_name_in);
  if(search != node_names_to_indexes.end())
    {
      throw std::logic_error("add_node() duplicate node name");
    }

  node_names_to_indexes[node_name_in] = node_names.size();
  node_names.push_back(node_name_in);
  assert(node_names.size() == node_names_to_indexes.size());

  node_edges.emplace_back();
  assert(node_edges.size() == node_names_to_indexes.size());
}

shared_dfa_ptr MoveGraph::get_moves(shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves");

  assert(node_names.size() >= 2);

  std::vector<DNFBuilder> node_builders(node_names.size(), positions_in->get_shape());

  node_builders[0].add_clause(DNFBuilder::clause_type(1, positions_in));

  for(int from_node_index = 0; from_node_index < node_names.size() - 1; ++from_node_index)
    {
      profile.tic("node inputs");

      // combine all positions coming into this node
      shared_dfa_ptr from_node_positions = node_builders[from_node_index].to_dfa();

      // sort edges leaving this node for pre-condition efficiency

      sort_edges(node_edges[from_node_index]);

      // process all edges leaving this node

      IntersectionManager manager(positions_in->get_shape());

      for(int node_edge_index = 0; node_edge_index < node_edges[from_node_index].size(); ++node_edge_index)
	{
	  profile.tic("edge init");
	  const move_edge& edge = node_edges[from_node_index][node_edge_index];

	  const std::string& edge_name = std::get<0>(edge);
	  const move_edge_condition_vector& pre_conditions = std::get<1>(edge);
	  const change_vector& changes = std::get<2>(edge);
	  const move_edge_condition_vector& post_conditions = std::get<3>(edge);
	  int to_node_index = std::get<4>(edge);

	  std::cout << " node " << from_node_index << "/" << node_names.size() << " (" << node_names[from_node_index] << "), edge " << node_edge_index << "/" << node_edges[from_node_index].size() << " (" << edge_name << ")" << std::endl;

	  shared_dfa_ptr edge_positions = from_node_positions;

	  // apply choice pre-conditions
	  profile.tic("edge pre conditions");
	  for(shared_dfa_ptr pre_condition : pre_conditions)
	    {
	      edge_positions = manager.intersect(edge_positions, pre_condition);
	      if(edge_positions->is_constant(false))
		{
		  // no matching positions
		  break;
		}
	    }
	  std::cout << "  pre-conditions => " << DFAUtil::quick_stats(edge_positions) << std::endl;

	  if(edge_positions->is_constant(false))
	    {
	      // no matching positions
	      continue;
	    }

	  // apply choice changes
	  profile.tic("edge change");
	  edge_positions = DFAUtil::get_change(edge_positions, changes);
	  std::cout << "  changes => " << DFAUtil::quick_stats(edge_positions) << std::endl;

	  if(edge_positions->is_constant(false))
	    {
	      // no matching positions
	      continue;
	    }

	  // add changed positions and post conditions to output builder

	  profile.tic("add output clause");

	  std::vector<shared_dfa_ptr> output_clause(post_conditions);
	  output_clause.push_back(edge_positions);
	  node_builders[to_node_index].add_clause(output_clause);
	}
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

MoveGraph MoveGraph::reverse() const
{
  MoveGraph output;

  for(int original_node_index = node_names.size() - 1; original_node_index >= 0; --original_node_index)
    {
      output.add_node(node_names[original_node_index]);
    }

  for(int original_from_node_index = node_names.size() - 1; original_from_node_index >= 0; --original_from_node_index)
    {
      for(const move_edge& original_edge : node_edges[original_from_node_index])
	{
	  const std::string& edge_name = std::get<0>(original_edge);
	  const move_edge_condition_vector& original_pre_conditions = std::get<1>(original_edge);
	  const change_vector& original_changes = std::get<2>(original_edge);
	  const move_edge_condition_vector& original_post_conditions = std::get<3>(original_edge);
	  int original_to_node_index = std::get<4>(original_edge);

	  change_vector reversed_changes;
	  for(int layer = 0; layer < original_changes.size(); ++layer)
	    {
	      const change_optional& original_change = original_changes[layer];

	      if(original_change.has_value())
		{
		  reversed_changes.emplace_back(change_type(std::get<1>(*original_change), std::get<0>(*original_change)));
		}
	      else
		{
		  reversed_changes.emplace_back();
		}
	    }

	  output.add_edge(edge_name,
			  node_names[original_to_node_index],
			  node_names[original_from_node_index],
			  original_post_conditions,
			  reversed_changes,
			  original_pre_conditions);
	}
    }

  return output;
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
