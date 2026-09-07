// Puzzle.h

#ifndef PUZZLE_H
#define PUZZLE_H

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "DFA.h"
#include "GameBase.h"
#include "MoveGraph.h"

class Puzzle
  : public GameBase
{
protected:

  Puzzle(std::string, const dfa_shape_t&);

  virtual shared_dfa_ptr build_positions_won() const = 0;

public:

  virtual ~Puzzle();

  // move generation

  shared_dfa_ptr get_moves_backward(shared_dfa_ptr positions_in) const {return GameBase::get_moves_backward(0, positions_in);}
  std::vector<DFAString> get_moves_forward(const DFAString& position_in) const {return GameBase::get_moves_forward(0, position_in);}
  shared_dfa_ptr get_moves_forward(shared_dfa_ptr positions_in) const {return GameBase::get_moves_forward(0, positions_in);}

  shared_dfa_ptr get_positions_won() const;
};

#endif
