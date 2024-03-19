// stats.cpp

#include <iostream>

#include "DFA.h"
#include "Game.h"
#include "utils.h"
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
  std::string hash_or_name(argv[2]);
  shared_dfa_ptr positions = get_dfa(game_name, hash_or_name);
  if(!positions)
    {
      std::cerr << hash_or_name << " not found." << std::endl;
      return 1;
    }

  std::cout << "states: " << positions->states() << std::endl;
  std::cout << "positions: " << positions->size() << std::endl;

  return 0;
}
