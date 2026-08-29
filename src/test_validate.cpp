// test_validate.cpp

#include <format>
#include <iostream>

#include <nlohmann/json.hpp>

#include "DFAUtil.h"
#include "test_utils.h"

void test_validate_case(const Game& game, const DFAString& position, const nlohmann::json& test_case)
{
  int side_to_move = test_case.at("side_to_move").get<int>();
  std::cout << "SIDE TO MOVE: " << side_to_move << std::endl;

  const auto validate_moves = game.validate_moves(side_to_move, position);
  const auto validate_result = game.validate_result(side_to_move, position);

  std::cout << "validate_moves() returned " << validate_moves.size() << " moves." << std::endl;
  if(validate_result)
    {
      std::cout << "validate_result() returned " << *validate_result << std::endl;
    }
  else
    {
      std::cout << "validate_result() returned none." << std::endl;
    }
  
  if(validate_moves.size() == 0)
    {
      if(!validate_result)
        {
          throw std::logic_error("validate_moves() returned zero moves, but validate_result() returned none.");
        }
    }
  else
    {
      if(validate_result)
        {
          throw std::logic_error(std::format("validate_moves() returned {:d} moves, but validate_result returned {:d}.", validate_moves.size(), *validate_result));
        }
    }

  if(test_case.contains("expected_result"))
    {
      nlohmann::json expected_result = test_case.at("expected_result");
      if(expected_result.is_null())
        {
          std::cout << "expected result none" << std::endl;
          if(validate_result)
            {
              throw std::logic_error(std::format("validate_result() returned {:d}, but expected none", *validate_result));
            }
        }
      else
        {
          std::cout << "expected result " << expected_result.get<int>() << std::endl;
          if(!validate_result)
            {
              throw std::logic_error(std::format("validate_result() returned none, but expected {:d}", expected_result.get<int>()));
            }
        }
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
