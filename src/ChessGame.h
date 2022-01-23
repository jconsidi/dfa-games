// ChessGame.h

#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include "Game.h"

enum ChessDFACharacter {DFA_BLANK, DFA_WHITE_KING, DFA_WHITE_QUEEN, DFA_WHITE_BISHOP, DFA_WHITE_KNIGHT, DFA_WHITE_ROOK, DFA_WHITE_PAWN, DFA_WHITE_PAWN_EN_PASSANT, DFA_BLACK_KING, DFA_BLACK_QUEEN, DFA_BLACK_BISHOP, DFA_BLACK_KNIGHT, DFA_BLACK_ROOK, DFA_BLACK_PAWN, DFA_BLACK_PAWN_EN_PASSANT, DFA_MAX};

#define CHESS_DFA_PARAMS 66, 64, 64, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX, DFA_MAX

class ChessGame : public Game<CHESS_DFA_PARAMS>
{
public:

  typedef typename Game<CHESS_DFA_PARAMS>::dfa_type dfa_type;
  typedef typename Game<CHESS_DFA_PARAMS>::shared_dfa_ptr shared_dfa_ptr;
  typedef typename Game<CHESS_DFA_PARAMS>::rule_vector rule_vector;

private:

  shared_dfa_ptr get_basic_positions() const;
  shared_dfa_ptr get_check_positions(int) const;
  shared_dfa_ptr get_king_positions(int) const;
  shared_dfa_ptr get_threat_positions(int, int) const;

public:

  virtual shared_dfa_ptr get_initial_positions() const;
  virtual shared_dfa_ptr get_lost_positions_helper(int) const;
  virtual const rule_vector& get_rules(int) const;
};

typedef DFA<CHESS_DFA_PARAMS> ChessDFA;

#endif
