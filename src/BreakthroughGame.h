// BreakthroughGame.h

#ifndef BREAKTHROUGH_GAME_H
#define BREAKTHROUGH_GAME_H

#include "NormalPlayGame.h"

class BreakthroughGame
: public NormalPlayGame
{
 private:

  int width;
  int height;

 public:

  BreakthroughGame(int, int);

  virtual MoveGraph build_move_graph(int) const;
  virtual DFAString get_position_initial() const;
  virtual std::string position_to_string(const DFAString&) const;
};

#endif
