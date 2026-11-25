// BreakthroughGame.h

#ifndef BREAKTHROUGH_GAME_H
#define BREAKTHROUGH_GAME_H

#include <string>
#include <vector>

#include "ConfigGame.h"
#include "RowMajorOrderBase.h"

class BreakthroughGame
  : public RowMajorOrderBase,
    public ConfigNormalPlayGame
{
protected:

  virtual shared_dfa_ptr build_positions_reversed(shared_dfa_ptr) const;

  // the side to move's moves, for validate_position_impl
  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;

public:

  BreakthroughGame(int, int);

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
