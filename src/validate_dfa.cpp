// validate_dfa.cpp

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

  // Loading already applies every check FORMAT-DFA.md requires of a reader:
  // magic, version, table consistency, and that the file is exactly as long
  // as its layout implies.
  shared_dfa_ptr dfa = get_dfa(game_name, hash_or_name);
  if(!dfa)
    {
      std::cerr << hash_or_name << " not found." << std::endl;
      return 1;
    }

  std::string hash_saved = dfa->get_hash();
  std::cout << "HASH SAVED = " << hash_saved << std::endl;

  // The optional check of section 7: that the digest describes the bytes.
  std::string hash_check = dfa->calculate_digest();
  std::cout << "HASH CHECK = " << hash_check << std::endl;

  if(hash_check != hash_saved)
    {
      std::cerr << "DIGEST MISMATCH" << std::endl;
      return 1;
    }

  std::cout << "canonical: " << (dfa->is_canonical() ? "yes" : "no") << std::endl;
  std::cout << "states: " << dfa->states() << std::endl;

  return 0;
}
