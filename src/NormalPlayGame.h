// NormalPlayGame.h

#ifndef NORMAL_PLAY_GAME_H
#define NORMAL_PLAY_GAME_H

#include "Game.h"

class NormalPlayGame
  : public Game
{
protected:

  NormalPlayGame(std::string, const dfa_shape_t&);

public:

  virtual shared_dfa_ptr build_positions_losing(int, int) const; // side to move loses in at most given ply
  virtual shared_dfa_ptr build_positions_lost(int) const; // side to move has lost, no moves available
  virtual shared_dfa_ptr build_positions_winning(int, int) const; // side to move wins in at most given ply

  // validation

  virtual int validate_result(int, DFAString) const;
};

#endif
