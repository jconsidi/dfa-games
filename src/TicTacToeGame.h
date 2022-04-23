// TicTacToeGame.h

#ifndef TICTACTOE_GAME_H
#define TICTACTOE_GAME_H

#include <string>

#include "AcceptDFA.h"
#include "FixedDFA.h"
#include "InverseDFA.h"
#include "Game.h"
#include "RejectDFA.h"
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

  typedef typename Game<n*n, shape_pack...>::dfa_type dfa_type;
  typedef typename Game<n*n, shape_pack...>::position_type position_type;
  typedef typename Game<n*n, shape_pack...>::shared_dfa_ptr shared_dfa_ptr;
  typedef typename Game<n*n, shape_pack...>::rule_vector rule_vector;

  typedef typename Game<n*n, shape_pack...>::intersection_dfa_type intersection_dfa_type;

 private:

  virtual shared_dfa_ptr get_initial_positions_internal() const;
  virtual shared_dfa_ptr get_lost_positions_internal(int) const;

  static shared_dfa_ptr get_lost_condition(int side_to_move, int x_start, int y_start, int x_delta, int y_delta);

 public:

  static std::string position_to_string(const position_type&);

  virtual const rule_vector& get_rules(int) const;
};

#define TICTACTOE2_SHAPE_PACK 3, 3, 3, 3
#define TICTACTOE2_DFA_PARAMS 4, TICTACTOE2_SHAPE_PACK
typedef class TicTacToeGame<2, TICTACTOE2_SHAPE_PACK> TicTacToe2Game;

#define TICTACTOE3_SHAPE_PACK 3, 3, 3, 3, 3, 3, 3, 3, 3
#define TICTACTOE3_DFA_PARAMS 9, TICTACTOE3_SHAPE_PACK
typedef class TicTacToeGame<3, TICTACTOE3_SHAPE_PACK> TicTacToe3Game;

#define TICTACTOE4_SHAPE_PACK 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
#define TICTACTOE4_DFA_PARAMS 16, TICTACTOE4_SHAPE_PACK
typedef class TicTacToeGame<4, TICTACTOE4_SHAPE_PACK> TicTacToe4Game;

#endif
