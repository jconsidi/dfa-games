// TicTacToeGame.h

#ifndef TICTACTOE_GAME_H
#define TICTACTOE_GAME_H

#include <string>

#include "AcceptDFA.h"
#include "FixedDFA.h"
#include "InverseDFA.h"
#include "Game.h"
#include "RejectDFA.h"
#include "TicTacToeDFAParams.h"
#include "UnionDFA.h"

template<int n, int... shape_pack>
  class TicTacToeGame : public Game<n*n, shape_pack...>
{
 private:

  typedef AcceptDFA<n*n, shape_pack...> accept_dfa_type;
  typedef FixedDFA<n*n, shape_pack...> fixed_dfa_type;
  typedef InverseDFA<n*n, shape_pack...> inverse_dfa_type;
  typedef RejectDFA<n*n, shape_pack...> reject_dfa_type;
  typedef UnionDFA<n*n, shape_pack...> union_dfa_type;

 public:

  TicTacToeGame();

  typedef typename Game<n*n, shape_pack...>::dfa_type dfa_type;
  typedef typename Game<n*n, shape_pack...>::dfa_string_type dfa_string_type;
  typedef typename Game<n*n, shape_pack...>::shared_dfa_ptr shared_dfa_ptr;
  typedef typename Game<n*n, shape_pack...>::rule_vector rule_vector;

  typedef typename Game<n*n, shape_pack...>::intersection_dfa_type intersection_dfa_type;

 private:

  virtual shared_dfa_ptr get_initial_positions_internal() const;
  virtual shared_dfa_ptr get_lost_positions_internal(int) const;
  virtual rule_vector get_rules_internal(int) const;

  static shared_dfa_ptr get_lost_condition(int side_to_move, int x_start, int y_start, int x_delta, int y_delta);

 public:

  virtual std::string position_to_string(const dfa_string_type&) const;
};

typedef class TicTacToeGame<2, TICTACTOE2_SHAPE_PACK> TicTacToe2Game;
typedef class TicTacToeGame<3, TICTACTOE3_SHAPE_PACK> TicTacToe3Game;
typedef class TicTacToeGame<4, TICTACTOE4_SHAPE_PACK> TicTacToe4Game;

#endif
