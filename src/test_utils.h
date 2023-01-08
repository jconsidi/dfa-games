// test_utils.h

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <vector>

#include "Game.h"

Game *get_game(std::string game_name);
void test_backward(const Game& game_in, int ply_max, bool initial_win_expected);
void test_forward(const Game& game_in, const std::vector<size_t>& positions_expected);
void test_game(const Game& game_in, const std::vector<size_t>& positions_expected, int ply_max, bool initial_win_expected);

#endif
