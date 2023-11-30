// Game.h

#ifndef GAME_H
#define GAME_H

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ChangeDFA.h"
#include "CountCharacterDFA.h"
#include "DFA.h"
#include "MoveGraph.h"

class Game
{
private:

  std::string name;
  dfa_shape_t shape;

  mutable MoveGraph move_graphs_forward[2] = {dfa_shape_t(), dfa_shape_t()};
  mutable MoveGraph move_graphs_backward[2] = {dfa_shape_t(), dfa_shape_t()};
  mutable bool move_graphs_ready[2] = {false, false};

  virtual MoveGraph build_move_graph(int) const = 0;
  void build_move_graphs(int) const;

  mutable shared_dfa_ptr singleton_has_moves[2] = {0, 0};

protected:

  Game(std::string, const dfa_shape_t&);
  virtual ~Game();

  int get_shape_size() const {return shape.size();}
  void save(std::string, shared_dfa_ptr) const;

public:

  // move generation
  shared_dfa_ptr get_has_moves(int) const;

  shared_dfa_ptr get_moves_backward(int, shared_dfa_ptr) const;
  shared_dfa_ptr get_moves_forward(int, shared_dfa_ptr) const;

  std::string get_name() const;
  std::string get_name_losing(int, int) const;
  std::string get_name_lost(int) const;
  std::string get_name_winning(int, int) const;
  std::string get_name_won(int) const;

  virtual DFAString get_position_initial() const = 0;

  shared_dfa_ptr get_positions_forward(int) const;
  shared_dfa_ptr get_positions_initial() const;
  virtual shared_dfa_ptr get_positions_losing(int, int) const; // side to move loses in at most given ply
  virtual shared_dfa_ptr get_positions_lost(int) const = 0; // side to move has lost, no moves available
  shared_dfa_ptr get_positions_reachable(int, int) const;
  virtual shared_dfa_ptr get_positions_winning(int, int) const; // side to move wins in at most given ply
  virtual shared_dfa_ptr get_positions_won(int) const = 0; // side to move has won, no moves available

  const dfa_shape_t& get_shape() const;

  // saved position access

  shared_dfa_ptr load(std::string dfa_name_in) const;
  shared_dfa_ptr load_by_hash(std::string) const;
  shared_dfa_ptr load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr()> build_func) const;

  // position evaluation

  virtual std::string position_to_string(const DFAString&) const = 0;
};

#endif
