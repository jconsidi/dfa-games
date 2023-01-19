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
typedef std::tuple<std::string, move_edge_condition_vector, change_vector, move_edge_condition_vector, int> move_edge;

class MoveGraph
{
 private:

  dfa_shape_t shape;

  std::vector<std::string> node_names;
  std::map<std::string, int> node_names_to_indexes;
  mutable std::vector<std::vector<move_edge>> node_edges; // mutable to sort

  std::set<std::string> edge_names;

  int get_node_index(std::string) const;

 public:

  MoveGraph(const dfa_shape_t&);

  void add_edge(std::string, std::string, std::string, const move_edge_condition_vector&, const change_vector&, const move_edge_condition_vector&);
  void add_node(std::string);

  shared_dfa_ptr get_moves(shared_dfa_ptr) const;

  MoveGraph reverse() const;
};

#endif
