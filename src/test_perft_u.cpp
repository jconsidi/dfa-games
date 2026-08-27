// test_perft_u.cpp

#include <cstdlib>
#include <iostream>

#include <nlohmann/json.hpp>

#include "test_utils.h"

void test_game(std::string game_name)
{
  std::cout << "############################################################" << std::endl;

  std::cout << "GAME: " << game_name << std::endl;
      
  Game *game = get_game(game_name);

  auto test_cases = get_test_cases(game_name, "perft_u");

  for(auto test_case : test_cases)
    {
      std::cout << "############################################################" << std::endl;
      
      std::vector<int> position_vector = test_case.at("position").get<std::vector<int>>();
      DFAString position(game->get_shape(), position_vector);

      std::cout << "POSITION:" << std::endl;
      std::cout << game->position_to_string(position) << std::endl;

      int side_to_move = test_case.at("side_to_move").get<int>();
      std::cout << "SIDE TO MOVE: " << side_to_move << std::endl;
      
      std::vector<int> expected = test_case.at("expected").get<std::vector<int>>();

      shared_dfa_ptr positions = game->get_positions_initial();
      for(int ply = 0; ply < expected.size(); ++ply)
        {
          int depth = ply + 1;
          positions = game->get_moves_forward((side_to_move + ply) % 2, positions);

          std::cout << "DEPTH: " << depth << ", POSITIONS: " << positions->size() << ", EXPECTED: " << expected[ply] << std::endl;

          if(positions->size() != expected[ply])
            {
              throw std::runtime_error("perft_u check failed");
            }
        }
    }
}

int main(int argc, char **argv)
{
  // TODO: support scanning for test cases if game name not specified
  
  if(argc < 2)
    {
      std::cerr << "usage: test_perft_u GAME_NAME\n";
      return 1;
    }

  std::string game_name(argv[1]);
  test_game(game_name);

  return 0;
}
