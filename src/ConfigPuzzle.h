// ConfigPuzzle.h

#ifndef CONFIG_PUZZLE_H
#define CONFIG_PUZZLE_H

#include "ConfigBase.h"
#include "DFA.h"
#include "MoveGraph.h"
#include "Puzzle.h"

class ConfigPuzzle
: protected ConfigBase,
  public Puzzle
{
 protected:

  virtual MoveGraph build_move_graph(int) const;
  virtual shared_dfa_ptr build_positions_won() const;

  dfa_shape_t get_shape() const {return GameBase::get_shape();}

public:

  ConfigPuzzle(std::string);
};

#endif
