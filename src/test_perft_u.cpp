// test_perft_u.cpp

#include <iostream>

#include <nlohmann/json.hpp>

#include "DFAUtil.h"
#include "test_utils.h"

void test_perft_u_case(const Game& game, const nlohmann::json& test_case)
{
  std::vector<int> position_vector = test_case.at("position").get<std::vector<int>>();
  DFAString position(game.get_shape(), position_vector);

  std::cout << "POSITION:" << std::endl;
  std::cout << game.position_to_string(position) << std::endl;

  int side_to_move = test_case.at("side_to_move").get<int>();
  std::cout << "SIDE TO MOVE: " << side_to_move << std::endl;

  std::vector<int> expected = test_case.at("expected").get<std::vector<int>>();

  shared_dfa_ptr positions = DFAUtil::from_string(game.get_shape(), position);
  for(int ply = 0; ply < expected.size(); ++ply)
    {
      int depth = ply + 1;
      positions = game.get_moves_forward((side_to_move + ply) % 2, positions);

      std::cout << "DEPTH: " << depth << ", POSITIONS: " << positions->size() << ", EXPECTED: " << expected[ply] << std::endl;

      if(positions->size() != expected[ply])
        {
          throw std::runtime_error("perft_u check failed");
        }
    }
}

int main(int argc, char **argv)
{
  if(argc > 2)
    {
      std::cerr << "usage: test_perft_u [GAME_NAME]\n";
      return 1;
    }

  run_test_cases("perft_u",
                 (argc >= 2) ? std::string(argv[1]) : std::string(""),
                 test_perft_u_case);

  return 0;
}
