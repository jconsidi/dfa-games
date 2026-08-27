// AmazonsGame.h

#ifndef AMAZONS_GAME_H
#define AMAZONS_GAME_H

#include <string>
#include <tuple>
#include <vector>

#include "NormalPlayGame.h"

class AmazonsGame
  : public NormalPlayGame
{
private:

  int width;
  int height;

  // Queen moves grouped by starting layer. Depends only on the board size,
  // so it is built once here rather than rebuilt for every position in
  // validate_moves().
  const std::vector<std::vector<std::tuple<int, std::vector<int>>>> queen_moves_by_layer;

  virtual MoveGraph build_move_graph(int) const;
  virtual shared_dfa_ptr build_positions_reversed(shared_dfa_ptr) const;

public:

  AmazonsGame(int, int);

  virtual DFAString get_position_initial() const;

  virtual std::string position_to_string(const DFAString&) const;

  // validation

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
};

#endif
