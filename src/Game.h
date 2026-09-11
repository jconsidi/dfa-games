// Game.h

#ifndef GAME_H
#define GAME_H

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "GameBase.h"

class Game
  : public GameBase
{
private:

  mutable std::optional<bool> reverse_implemented;

  mutable shared_dfa_ptr singleton_has_moves[2] = {0, 0};

protected:

  Game(std::string, const dfa_shape_t&);

  // move generation

  virtual shared_dfa_ptr build_positions_losing(int, int) const;
  virtual shared_dfa_ptr build_positions_lost(int) const;
  virtual shared_dfa_ptr build_positions_reversed(shared_dfa_ptr) const;
  virtual shared_dfa_ptr build_positions_winning(int, int) const;
  virtual shared_dfa_ptr build_positions_won(int) const;

public:

  // move generation

  bool can_reverse() const;

  shared_dfa_ptr get_has_moves(int) const;

  std::string get_name_losing(int, int) const;
  std::string get_name_lost(int) const;
  std::string get_name_unknown(int, int) const;
  std::string get_name_winning(int, int) const;
  std::string get_name_won(int) const;

  virtual DFAString get_position_initial() const = 0;

  shared_dfa_ptr get_positions_forward(int) const;
  shared_dfa_ptr get_positions_initial() const;
  shared_dfa_ptr get_positions_losing(int, int) const; // side to move loses in at most given ply
  shared_dfa_ptr get_positions_lost(int) const; // side to move has lost, no moves available
  shared_dfa_ptr get_positions_reachable(int, int) const;
  shared_dfa_ptr get_positions_unknown(int, int) const; // side to move does not have win or loss within given ply
  shared_dfa_ptr get_positions_winning(int, int) const; // side to move wins in at most given ply
  shared_dfa_ptr get_positions_won(int) const; // side to move has won, no moves available

  // saved position access

  // durable defaults true here (unlike GameBase's/DFAUtil's own default of
  // false): most names a Game asks for by hand are meaningful solver
  // results (lost, won, backward,ply_max=*, ...), so the common case at
  // this layer should not have to say so, and the rare cache-like
  // exception should. Hides GameBase's own (durable-less) overloads.
  shared_dfa_ptr load_by_hash(std::string, bool durable = true) const;
  shared_dfa_ptr load_by_name(std::string dfa_name_in, bool durable = true) const;
  shared_dfa_ptr load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr()> build_func, bool durable = true) const;
};

#endif
