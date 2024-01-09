// NormalNimGame.h

#ifndef NORMAL_NIM_GAME_H
#define NORMAL_NIM_GAME_H

#include "NormalPlayGame.h"
#include "NormalNimDFAParams.h"

class NormalNimGame
  : public NormalPlayGame
{
private:

  virtual MoveGraph build_move_graph(int) const;

public:

  NormalNimGame(const dfa_shape_t&);

  virtual DFAString get_position_initial() const;

  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, DFAString) const;
};

#endif
