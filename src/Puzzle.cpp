// Puzzle.cpp

#include "Puzzle.h"

Puzzle::Puzzle(std::string name_in, const dfa_shape_t& shape_in)
  : GameBase(name_in, shape_in, 1)
{
}

Puzzle::~Puzzle()
{
}

shared_dfa_ptr Puzzle::get_positions_won() const
{
  return load_or_build("won", [&]()
  {
    return build_positions_won();
  });
}
