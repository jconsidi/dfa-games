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

  std::cout << "move graph moves: " << actual_moves_set.size() << ", validation moves: " << validate_moves_set.size() << std::endl;

  std::set<DFAString> extra_moves;
  std::set_difference(actual_moves_set.begin(), actual_moves_set.end(),
                      validate_moves_set.begin(), validate_moves_set.end(),
                      std::inserter(extra_moves, extra_moves.end()));
  for(const DFAString& extra_move : extra_moves)
    {
      std::cerr << "EXTRA MOVE FOUND:" << std::endl;
      std::cerr << game.position_to_string(extra_move) << std::endl;
      throw std::logic_error(std::format("found {:d} extra moves", extra_moves.size()));
    }

  std::set<DFAString> missing_moves;
  std::set_difference(validate_moves_set.begin(), validate_moves_set.end(),
                      actual_moves_set.begin(), actual_moves_set.end(),
                      std::inserter(missing_moves, missing_moves.end()));
  for(const DFAString& missing_move : missing_moves)
    {
      std::cerr << "MISSING MOVE FOUND:" << std::endl;
      std::cerr << game.position_to_string(missing_move) << std::endl;
      throw std::logic_error(std::format("found {:d} missing moves", missing_moves.size()));
    }
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
