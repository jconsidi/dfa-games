// test_move_graph.cpp

#include <algorithm>
#include <format>
#include <iostream>
#include <set>

#include <nlohmann/json.hpp>

#include "DFAUtil.h"
#include "test_utils.h"

void test_position(const Game& game, const DFAString& position, const nlohmann::json& test_case)
{
  int side_to_move = test_case.at("side_to_move").get<int>();
  std::cout << "SIDE TO MOVE: " << side_to_move << std::endl;

  std::vector<DFAString> actual_moves = game.get_moves_forward(side_to_move, position);

  std::vector<DFAString> validate_moves = game.validate_moves(side_to_move, position);

  std::set<DFAString> actual_moves_set(actual_moves.begin(), actual_moves.end());
  std::set<DFAString> validate_moves_set(validate_moves.begin(), validate_moves.end());

  test_moves(game, actual_moves_set, validate_moves_set);
}

int main(int argc, char **argv)
{
  if(argc > 2)
    {
      std::cerr << "usage: test_move_graph [GAME_NAME]\n";
      return 1;
    }

  run_test_positions((argc >= 2) ? std::string(argv[1]) : std::string(""),
                     test_position);

  return 0;
}
