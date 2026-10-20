// ClobberGame.h

#ifndef CLOBBER_GAME_H
#define CLOBBER_GAME_H

#include <string>

#include "ConfigGame.h"
#include "RowMajorOrderBase.h"

class ClobberGame
  : public RowMajorOrderBase,
    public ConfigNormalPlayGame
{
private:

#if 0
  virtual shared_dfa_ptr build_positions_reversed(shared_dfa_ptr) const;
#endif

public:

  ClobberGame(int, int);

  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
};

#endif
