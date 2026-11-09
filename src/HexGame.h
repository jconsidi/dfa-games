// HexGame.h

#ifndef HEX_GAME_H
#define HEX_GAME_H

#include <string>

#include "ConfigGame.h"
#include "RowMajorOrderBase.h"

class HexGame
: public RowMajorOrderBase,
  public ConfigNormalPlayGame
{
private:

  int n;

  bool check_connected(int, const DFAString&) const;

public:

  HexGame(int);

  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
};

#endif
