// ChessGame.h

#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include "Board.h"
#include "ChessDFAParams.h"
#include "Game.h"

class ChessGame : public Game<CHESS_DFA_PARAMS>
{
private:

  virtual shared_dfa_ptr get_initial_positions_internal() const;
  virtual shared_dfa_ptr get_lost_positions_internal(int) const;
  virtual rule_vector get_rules_internal(int) const;

  shared_dfa_ptr get_basic_positions() const;
  shared_dfa_ptr get_king_positions(int) const;

public:

  ChessGame();

  static shared_dfa_ptr from_board(const Board& board);

  shared_dfa_ptr get_check_positions(int) const;
  shared_dfa_ptr get_threat_positions(int, int) const;

  virtual std::string position_to_string(const dfa_string_type&) const;
};

#endif
