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

template <int ndim, int... shape_pack>
class Game
{
private:

  std::string name;

public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef DFAString<ndim, shape_pack...> dfa_string_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;

  typedef ChangeDFA<ndim, shape_pack...> change_dfa_type;
  typedef CountCharacterDFA<ndim, shape_pack...> count_character_dfa_type;

  typedef std::tuple<std::vector<shared_dfa_ptr>, change_vector, std::vector<shared_dfa_ptr>, std::string> choice_type;
  typedef std::vector<choice_type> choice_vector;
  typedef std::vector<choice_vector> phase_vector;

private:

  mutable shared_dfa_ptr singleton_has_moves[2] = {0, 0};
  mutable phase_vector singleton_phases_backward[2] = {};
  mutable phase_vector singleton_phases_forward[2] = {};

  const phase_vector& get_phases_backward(int) const;
  const phase_vector& get_phases_forward(int) const;
  virtual phase_vector get_phases_internal(int) const = 0;

  shared_dfa_ptr get_moves_internal(const phase_vector&, shared_dfa_ptr) const;

protected:

  Game(std::string);

  void save(std::string, shared_dfa_ptr) const;
  shared_dfa_ptr load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr()> build_func) const;

public:

  // move generation
  shared_dfa_ptr get_has_moves(int) const;

  shared_dfa_ptr get_moves_backward(int, shared_dfa_ptr) const;
  shared_dfa_ptr get_moves_forward(int, shared_dfa_ptr) const;

  virtual shared_dfa_ptr get_positions_initial() const = 0;
  virtual shared_dfa_ptr get_positions_losing(int, int) const; // side to move loses in at most given ply
  virtual shared_dfa_ptr get_positions_lost(int) const = 0; // side to move has lost, no moves available
  virtual shared_dfa_ptr get_positions_winning(int, int) const; // side to move wins in at most given ply
  virtual shared_dfa_ptr get_positions_won(int) const = 0; // side to move has won, no moves available

  // position evaluation

  virtual std::string position_to_string(const dfa_string_type&) const = 0;
};

#endif
