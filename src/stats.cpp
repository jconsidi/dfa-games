// stats.cpp

#include <iostream>

#include "DFA.h"
#include "Game.h"
#include "chess.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc != 3)
    {
      std::cerr << "usage: " << argv[0] << " GAME DFA_NAME" << std::endl;
      return 1;
    }

  // args = game, DFA name
  std::string game_name(argv[1]);
  std::string positions_name(argv[2]);
  shared_dfa_ptr positions = get_dfa(game_name, positions_name);
  if(!positions)
    {
      std::cerr << positions_name << " not found." << std::endl;
      return 1;
    }

  std::cout << "states: " << positions->states() << std::endl;
  std::cout << "positions: " << positions->size() << std::endl;

  return 0;
}
