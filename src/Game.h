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
#include "DedupedDFA.h"
#include "DifferenceDFA.h"

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
  typedef DedupedDFA<ndim, shape_pack...> deduped_dfa_type;
  typedef DifferenceDFA<ndim, shape_pack...> difference_dfa_type;

  typedef std::tuple<std::vector<shared_dfa_ptr>, change_func, std::vector<shared_dfa_ptr>, std::string> rule_type;
  typedef std::vector<rule_type> rule_vector;

private:

  mutable shared_dfa_ptr singleton_has_moves[2] = {0, 0};
  mutable shared_dfa_ptr singleton_initial_positions = 0;
  mutable shared_dfa_ptr singleton_lost_positions[2] = {0, 0};
  mutable rule_vector singleton_rules[2] = {};

  virtual shared_dfa_ptr get_initial_positions_internal() const = 0;
  virtual shared_dfa_ptr get_lost_positions_internal(int) const = 0;
  virtual rule_vector get_rules_internal(int) const = 0;

  shared_dfa_ptr get_moves_internal(const rule_vector&, shared_dfa_ptr) const;

protected:

  Game(std::string);

  void save(std::string, shared_dfa_ptr) const;
  shared_dfa_ptr load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr()> build_func) const;

public:

  shared_dfa_ptr get_has_moves(int) const;
  shared_dfa_ptr get_initial_positions() const;
  const rule_vector& get_rules(int) const;

  shared_dfa_ptr get_lost_positions(int) const;
  shared_dfa_ptr get_moves_forward(int, shared_dfa_ptr) const;
  shared_dfa_ptr get_moves_reverse(int, shared_dfa_ptr) const;
  shared_dfa_ptr get_winning_positions(int, int) const;
  shared_dfa_ptr get_winning_positions(int, int, shared_dfa_ptr) const;

  virtual std::string position_to_string(const dfa_string_type&) const = 0;
};

#endif
