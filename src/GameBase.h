// GameBase.h

#ifndef GAME_BASE_H
#define GAME_BASE_H

#include <memory>

#include "DFA.h"
#include "MoveGraph.h"

class GameBase
{
private:

  std::string name;
  dfa_shape_t shape;
  int sides;

  mutable std::vector<MoveGraph *> move_graphs_forward;
  mutable std::vector<MoveGraph *> move_graphs_backward;

protected:

  GameBase(std::string, const dfa_shape_t&, int);

  virtual MoveGraph build_move_graph(int) const = 0;
  
  int get_shape_size() const {return int(shape.size());}

public:

  virtual ~GameBase();

  const MoveGraph& get_move_graph_forward(int) const;
  const MoveGraph& get_move_graph_backward(int) const;

  shared_dfa_ptr get_moves_backward(int, shared_dfa_ptr) const;
  std::vector<DFAString> get_moves_forward(int, const DFAString&) const;
  shared_dfa_ptr get_moves_forward(int, shared_dfa_ptr) const;

  std::string get_name() const {return name;}
  const dfa_shape_t& get_shape() const {return shape;}

  // saved position access

  shared_dfa_ptr load(std::string dfa_name_in) const;
  shared_dfa_ptr load_by_hash(std::string) const;
  shared_dfa_ptr load_by_name(std::string dfa_name_in) const;
  shared_dfa_ptr load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr()> build_func) const;

  // position evaluation

  virtual std::string position_to_string(const DFAString&) const = 0;

  // validation

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
  virtual std::optional<int> validate_result(int, const DFAString&) const;
};

#endif
