// NormalPlayGame.h

#ifndef NORMAL_PLAY_GAME_H
#define NORMAL_PLAY_GAME_H

#include "Game.h"

class NormalPlayGame
  : public Game
{
protected:

  NormalPlayGame(std::string, const dfa_shape_t&);

  // A position with no moves is lost; otherwise its moves. Generates the
  // moves once.
  virtual ValidatedPosition validate_position_impl(int, const DFAString&) const;

  // The side to move's moves, for validate_position_impl.
  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const = 0;

public:

  virtual shared_dfa_ptr build_positions_losing(int, int) const; // side to move loses in at most given ply
  virtual shared_dfa_ptr build_positions_lost(int) const; // side to move has lost, no moves available
  virtual shared_dfa_ptr build_positions_winning(int, int) const; // side to move wins in at most given ply
};

#endif
