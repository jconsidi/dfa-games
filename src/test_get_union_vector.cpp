// test_get_union.cpp

#include <cstdlib>
#include <iostream>
#include <vector>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: test_forward GAME_NAME <HASHES>\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  std::vector<shared_dfa_ptr> dfas;
  for(int i = 2; i < argc; ++i)
    {
      std::string dfa_hash(argv[i]);
      shared_dfa_ptr dfa = game->load_by_hash(dfa_hash);
      if(dfa == 0)
	{
	  std::cerr << "DFA not found for hash " << dfa_hash << "." << std::endl;
	  return 1;
	}

      dfas.push_back(dfa);
    }

  shared_dfa_ptr test_union = DFAUtil::get_union_vector(dfas[0]->get_shape(), dfas);
  assert(test_union != 0);

  return 0;
}
