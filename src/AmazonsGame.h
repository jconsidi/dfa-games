// AmazonsGame.h

#ifndef NORMAL_NIM_GAME_H
#define NORMAL_NIM_GAME_H

#include <string>

#include "NormalPlayGame.h"

class AmazonsGame
  : public NormalPlayGame
{
private:

  int width;
  int height;

  virtual MoveGraph build_move_graph(int) const;

public:

  AmazonsGame(int, int);

  virtual shared_dfa_ptr get_positions_initial() const;

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
