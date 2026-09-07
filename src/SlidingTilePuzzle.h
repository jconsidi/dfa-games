// SlidingTilePuzzle.h

#ifndef SLIDING_TILE_PUZZLE_H
#define SLIDING_TILE_PUZZLE_H

#include "ConfigPuzzle.h"
#include "RowMajorOrderBase.h"

class SlidingTilePuzzle
: public RowMajorOrderBase,
  public ConfigPuzzle
{
 public:

  SlidingTilePuzzle(int, int);

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
