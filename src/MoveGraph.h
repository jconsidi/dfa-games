// MoveGraph.h

#ifndef MOVE_GRAPH_H
#define MOVE_GRAPH_H

// implements an acyclic multigraph representing all possible moves in a game.

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "ChangeDFA.h"
#include "DFA.h"

typedef std::vector<shared_dfa_ptr> move_edge_condition_vector;
typedef std::tuple<std::string, move_edge_condition_vector, int> move_edge;

class MoveGraph
{
 private:

  dfa_shape_t shape;

  std::vector<std::string> node_names;
  std::map<std::string, int> node_names_to_indexes;
  std::vector<change_vector> node_changes;

  // used to simplify edge setup
  std::vector<std::vector<shared_dfa_ptr>> node_pre_conditions;
  std::vector<std::vector<shared_dfa_ptr>> node_post_conditions;

  std::vector<std::vector<move_edge>> node_edges;
  std::vector<std::vector<std::pair<int, move_edge>>> node_inputs;

  std::set<std::string> edge_names;

  int get_node_index(std::string) const;

 public:

  MoveGraph(const dfa_shape_t&);

  void add_edge(std::string, std::string);
  void add_edge(std::string, std::string, std::string);
  void add_edge(std::string, std::string, std::string, const move_edge_condition_vector&);
  void add_edge(std::string, std::string, std::string, shared_dfa_ptr);

  void add_node(std::string);
  void add_node(std::string, const change_vector&);
  void add_node(std::string, const change_vector&, const move_edge_condition_vector&, const move_edge_condition_vector&);
  void add_node(std::string, int, int, int);

  shared_dfa_ptr get_moves(std::string name_prefix, shared_dfa_ptr) const;
  const move_edge_condition_vector& get_node_pre_conditions(std::string) const;

  MoveGraph optimize() const;
  MoveGraph reverse() const;
  size_t size() const;
};

#endif
