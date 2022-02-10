// test_utils.h

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "Game.h"

template<int ndim, int... shape_pack>
  void test_game(const Game<ndim, shape_pack...>& game_in);

#endif
