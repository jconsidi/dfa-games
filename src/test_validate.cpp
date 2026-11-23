// test_validate.cpp

#include <format>
#include <iostream>
#include <set>

#include <nlohmann/json.hpp>

#include "DFAUtil.h"
#include "test_utils.h"

void test_validate_case(const GameBase& game, const DFAString& position, const nlohmann::json& test_case)
{
  int side_to_move = test_case.at("side_to_move").get<int>();
  std::cout << "SIDE TO MOVE: " << side_to_move << std::endl;

  // validate_position throws if the rules give both moves and a result, or
  // neither.
  const ValidatedPosition validated = game.validate_position(side_to_move, position);
  const GameOver *over = std::get_if<GameOver>(&validated);
  const std::vector<DFAString> moves = validated_moves(validated);

  if(over)
    {
      std::cout << "validate_position() returned game over with result " << over->result << std::endl;
    }
  else
    {
      std::cout << "validate_position() returned " << moves.size() << " moves." << std::endl;
    }

  if(test_case.contains("expected_result"))
    {
      nlohmann::json expected_result = test_case.at("expected_result");
      if(expected_result.is_null())
        {
          std::cout << "expected result none" << std::endl;
          if(over)
            {
              throw std::logic_error(std::format("validate_position() returned result {:d}, but expected none", over->result));
            }
        }
      else
        {
          std::cout << "expected result " << expected_result.get<int>() << std::endl;
          if(!over)
            {
              throw std::logic_error(std::format("validate_position() returned moves, but expected result {:d}", expected_result.get<int>()));
            }
        }
    }

  if(test_case.contains("expected_moves"))
    {
      std::vector<std::vector<int>> expected_moves = test_case.at("expected_moves").get<std::vector<std::vector<int>>>();
      std::set<DFAString> expected_moves_set;
      for(const std::vector<int>& expected_move_vector : expected_moves)
        {
          expected_moves_set.emplace(game.get_shape(), expected_move_vector);
        }

      std::set<DFAString> validated_moves_set(moves.begin(), moves.end());

      test_moves(game, validated_moves_set, expected_moves_set);
    }
}

int main(int argc, char **argv)
{
  if(argc > 2)
    {
      std::cerr << "usage: test_validate [GAME_NAME]\n";
      return 1;
    }

  run_test_positions((argc >= 2) ? std::string(argv[1]) : std::string(""),
                     test_validate_case);

  return 0;
}
