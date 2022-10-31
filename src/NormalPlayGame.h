// NormalPlayGame.h

#ifndef NORMAL_PLAY_GAME_H
#define NORMAL_PLAY_GAME_H

#include "Game.h"

template<int ndim, int... shape_pack>
class NormalPlayGame
  : public Game<ndim, shape_pack...>
{
public:

  typedef typename Game<ndim, shape_pack...>::shared_dfa_ptr shared_dfa_ptr;

protected:

  NormalPlayGame(std::string);

public:

  virtual shared_dfa_ptr get_positions_initial() const = 0;
  virtual shared_dfa_ptr get_positions_losing(int, int) const; // side to move loses in at most given ply
  virtual shared_dfa_ptr get_positions_lost(int) const; // side to move has lost, no moves available
  virtual shared_dfa_ptr get_positions_winning(int, int) const; // side to move wins in at most given ply
  virtual shared_dfa_ptr get_positions_won(int) const; // side to move has won, no moves available
};

#endif
