// validate_dfa.cpp

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
  std::string hash_or_name(argv[2]);

  shared_dfa_ptr dfa = get_dfa(game_name, hash_or_name);

  std::string hash_saved = dfa->get_hash();
  std::cout << "HASH SAVED = " << hash_saved << std::endl;

  std::string hash_check = dfa->calculate_hash();
  std::cout << "HASH CHECK = " << hash_check << std::endl;

  assert(hash_check == hash_saved);

  return 0;
}
