// test_utils.h

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <vector>

#include "Game.h"

template<int ndim, int... shape_pack>
void test_backward(const Game<ndim, shape_pack...>& game_in, int moves_max, bool initial_win_expected);
template<int ndim, int... shape_pack>
void test_forward(const Game<ndim, shape_pack...>& game_in, const std::vector<size_t>& positions_expected);
template<int ndim, int... shape_pack>
void test_game(const Game<ndim, shape_pack...>& game_in, const std::vector<size_t>& positions_expected, int moves_max, bool initial_win_expected);

#endif
