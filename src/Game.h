// Game.h

#ifndef GAME_H
#define GAME_H

#include <memory>
#include <tuple>
#include <vector>

#include "AcceptDFA.h"
#include "ChangeDFA.h"
#include "DFA.h"
#include "DifferenceDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "UnionDFA.h"

template <int ndim, int... shape_pack>
class Game
{
public:

  typedef DFA<ndim, shape_pack...> dfa_type;
  typedef std::shared_ptr<const dfa_type> shared_dfa_ptr;

  typedef AcceptDFA<ndim, shape_pack...> accept_dfa_type;
  typedef ChangeDFA<ndim, shape_pack...> change_dfa_type;
  typedef DifferenceDFA<ndim, shape_pack...> difference_dfa_type;
  typedef IntersectionDFA<ndim, shape_pack...> intersection_dfa_type;
  typedef InverseDFA<ndim, shape_pack...> inverse_dfa_type;
  typedef UnionDFA<ndim, shape_pack...> union_dfa_type;

  typedef std::tuple<shared_dfa_ptr, change_func, shared_dfa_ptr> rule_type;
  typedef std::vector<rule_type> rule_vector;

private:

  shared_dfa_ptr get_moves_internal(const rule_vector&, const dfa_type&) const;

protected:

  Game();

public:

  virtual shared_dfa_ptr get_initial_positions() const = 0;
  virtual shared_dfa_ptr get_lost_positions_helper(int) const = 0;
  virtual rule_vector get_rules(int) const = 0;

  shared_dfa_ptr get_lost_positions(int) const;
  shared_dfa_ptr get_lost_positions(int, const dfa_type&) const;
  shared_dfa_ptr get_moves_forward(int, const dfa_type&) const;
  shared_dfa_ptr get_moves_reverse(int, const dfa_type&) const;
  shared_dfa_ptr get_winning_positions(int, int) const;
  shared_dfa_ptr get_winning_positions(int, int, const dfa_type&) const;
  shared_dfa_ptr get_winning_positions(int, int, const dfa_type* = 0) const;
};

#endif
