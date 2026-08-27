// test_utils.h

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Game.h"

struct TestGroup
{
  std::string game_name;
  std::vector<nlohmann::json> test_cases;
};

bool check_loss(const Game& game, int ply_max);
bool check_win(const Game& game, int ply_max);

shared_dfa_ptr get_dfa(std::string game_name, std::string hash_or_name);
Game *get_game(std::string game_name);
// game_name == "" means every game with a config/<game>/tests.json
std::vector<TestGroup> get_test_cases(std::string test_type, std::string game_name = "");
void run_test_cases(std::string test_type, std::string game_name, std::function<void(const Game&, const nlohmann::json&)> test_case_func);
void test_backward(const Game& game_in, int ply_max, bool initial_win_expected);
void test_forward(const Game& game_in, const std::vector<size_t>& positions_expected);
void test_game(const Game& game_in, const std::vector<size_t>& positions_expected, int ply_max, bool initial_win_expected);

#endif
