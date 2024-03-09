// AmazonsGame.h

#ifndef AMAZONS_GAME_H
#define AMAZONS_GAME_H

#include <string>

#include "NormalPlayGame.h"

class AmazonsGame
  : public NormalPlayGame
{
private:

  int width;
  int height;

  virtual MoveGraph build_move_graph(int) const;

protected:

  // move generation

  virtual shared_dfa_ptr build_positions_forward_bound(int) const;

public:

  AmazonsGame(int, int);

  virtual DFAString get_position_initial() const;

  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, DFAString) const;
};

#endif
