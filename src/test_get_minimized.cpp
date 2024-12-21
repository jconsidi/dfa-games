// test_get_minimized.cpp

#include <cstdlib>
#include <iostream>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 3)
    {
      std::cerr << "usage: test_get_minimized GAME_NAME HASH\n";
      return 1;
    }

  std::string game_name(argv[1]);

  std::string hash_or_name(argv[2]);
  shared_dfa_ptr dfa_in = get_dfa(game_name, hash_or_name);
  if(dfa_in == 0)
    {
      std::cerr << "DFA not found for hash or name " << hash_or_name << "." << std::endl;
      return 1;
    }

  shared_dfa_ptr test_minimized = DFAUtil::get_minimized(dfa_in);
  assert(test_minimized != 0);

  return 0;
}
