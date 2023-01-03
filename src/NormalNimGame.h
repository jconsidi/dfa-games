// NormalNimGame.h

#ifndef NORMAL_NIM_GAME_H
#define NORMAL_NIM_GAME_H

#include "NormalPlayGame.h"
#include "NormalNimDFAParams.h"

class NormalNimGame
  : public NormalPlayGame
{
private:

  virtual phase_vector get_phases_internal(int) const;

public:

  NormalNimGame(const dfa_shape_t&);

  virtual shared_dfa_ptr get_positions_initial() const;

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
