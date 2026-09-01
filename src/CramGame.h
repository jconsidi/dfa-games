// CramGame.h

#ifndef CRAM_GAME_H
#define CRAM_GAME_H

#include <string>

#include "ConfigGame.h"
#include "RowMajorOrderBase.h"

class CramGame
: public RowMajorOrderBase,
  public ConfigNormalPlayGame
{
public:

  CramGame(int, int);

  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
};

#endif
