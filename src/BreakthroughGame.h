// BreakthroughGame.h

#ifndef BREAKTHROUGH_GAME_H
#define BREAKTHROUGH_GAME_H

#include "ColumnMajorOrderBase.h"
#include "NormalPlayGame.h"
#include "RowMajorOrderBase.h"

class BreakthroughBase
  : virtual public RectangularBase,
    public NormalPlayGame
    
{
protected:

  BreakthroughBase(std::string, int, int);

  virtual shared_dfa_ptr build_positions_reversed(shared_dfa_ptr) const;

public:

  virtual MoveGraph build_move_graph(int) const;
  virtual DFAString get_position_initial() const;
  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
};

class BreakthroughColumnWiseGame
  : public ColumnMajorOrderBase,
    public BreakthroughBase
{
public:

  BreakthroughColumnWiseGame(int, int);
};

class BreakthroughRowWiseGame
  : public RowMajorOrderBase,
    public BreakthroughBase
{
public:

  BreakthroughRowWiseGame(int, int);
};

typedef BreakthroughRowWiseGame BreakthroughGame;

#endif
