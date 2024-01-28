// BreakthroughGame.h

#ifndef BREAKTHROUGH_GAME_H
#define BREAKTHROUGH_GAME_H

#include "NormalPlayGame.h"

class BreakthroughBase
  : public NormalPlayGame
{
protected:

  int width;
  int height;

  BreakthroughBase(std::string, int, int);

  virtual int calculate_layer(int row, int column) const = 0;

public:

  virtual MoveGraph build_move_graph(int) const;
  virtual DFAString get_position_initial() const;
  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, DFAString) const;
};

class BreakthroughColumnWiseGame
  : public BreakthroughBase
{
protected:

  virtual int calculate_layer(int row, int column) const;

public:

    BreakthroughColumnWiseGame(int, int);
};

class BreakthroughRowWiseGame
  : public BreakthroughBase
{
protected:

  virtual int calculate_layer(int row, int column) const;

public:

  BreakthroughRowWiseGame(int, int);
};

typedef BreakthroughRowWiseGame BreakthroughGame;

#endif
