// NormalNimGame.h

#ifndef NORMAL_NIM_GAME_H
#define NORMAL_NIM_GAME_H

#include "NormalPlayGame.h"
#include "NormalNimDFAParams.h"

template <int ndim, int... shape_pack>
class NormalNimGame
  : public NormalPlayGame<ndim, shape_pack...>
{
public:

  typedef typename Game<ndim, shape_pack...>::dfa_string_type dfa_string_type;
  typedef typename Game<ndim, shape_pack...>::shared_dfa_ptr shared_dfa_ptr;

  typedef typename Game<ndim, shape_pack...>::rule_vector rule_vector;
  typedef typename Game<ndim, shape_pack...>::phase_vector phase_vector;

private:

  virtual phase_vector get_phases_internal(int) const;

public:

  NormalNimGame();

  virtual shared_dfa_ptr get_positions_initial() const;

  virtual std::string position_to_string(const dfa_string_type&) const;
};

typedef class NormalNimGame<1, NORMALNIM1_SHAPE_PACK> NormalNim1Game;
typedef class NormalNimGame<2, NORMALNIM2_SHAPE_PACK> NormalNim2Game;
typedef class NormalNimGame<3, NORMALNIM3_SHAPE_PACK> NormalNim3Game;
typedef class NormalNimGame<4, NORMALNIM4_SHAPE_PACK> NormalNim4Game;

#endif
