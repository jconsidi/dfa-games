// Game.h

#ifndef GAME_H
#define GAME_H

#include <memory>
#include <tuple>
#include <vector>

#include "AcceptDFA.h"
#include "ChangeDFA.h"
#include "CountCharacterDFA.h"
#include "DFA.h"
#include "DifferenceDFA.h"
#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "RejectDFA.h"
#include "UnionDFA.h"

template <int ndim, int... shape_pack>
class Game
{
public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;

  typedef AcceptDFA<ndim, shape_pack...> accept_dfa_type;
  typedef ChangeDFA<ndim, shape_pack...> change_dfa_type;
  typedef CountCharacterDFA<ndim, shape_pack...> count_character_dfa_type;
  typedef DifferenceDFA<ndim, shape_pack...> difference_dfa_type;
  typedef FixedDFA<ndim, shape_pack...> fixed_dfa_type;
  typedef IntersectionDFA<ndim, shape_pack...> intersection_dfa_type;
  typedef InverseDFA<ndim, shape_pack...> inverse_dfa_type;
  typedef RejectDFA<ndim, shape_pack...> reject_dfa_type;
  typedef UnionDFA<ndim, shape_pack...> union_dfa_type;

  typedef std::tuple<shared_dfa_ptr, change_func, shared_dfa_ptr> rule_type;
  typedef std::vector<rule_type> rule_vector;

private:

  mutable shared_dfa_ptr singleton_has_moves[2] = {0, 0};
  mutable shared_dfa_ptr singleton_initial_positions = 0;
  mutable shared_dfa_ptr singleton_lost_positions[2] = {0, 0};

  virtual shared_dfa_ptr get_initial_positions_internal() const = 0;
  virtual shared_dfa_ptr get_lost_positions_internal(int) const = 0;

  shared_dfa_ptr get_moves_internal(const rule_vector&, shared_dfa_ptr) const;

protected:

  Game();

public:

  shared_dfa_ptr get_has_moves(int) const;
  shared_dfa_ptr get_initial_positions() const;
  virtual const rule_vector& get_rules(int) const = 0;

  shared_dfa_ptr get_lost_positions(int) const;
  shared_dfa_ptr get_moves_forward(int, shared_dfa_ptr) const;
  shared_dfa_ptr get_moves_reverse(int, shared_dfa_ptr) const;
  shared_dfa_ptr get_winning_positions(int, int) const;
  shared_dfa_ptr get_winning_positions(int, int, shared_dfa_ptr) const;
};

#endif
