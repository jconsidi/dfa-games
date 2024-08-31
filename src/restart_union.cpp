// restart_union.cpp

#include <cstdlib>
#include <iostream>

#include "DFAUtil.h"
#include "UnionRestartDFA.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: restart_union GAME_NAME LEFT_HASH RIGHT_HASH\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  std::string left_hash(argv[2]);
  shared_dfa_ptr left = game->load_by_hash(left_hash);
  if(left == 0)
    {
      std::cerr << "DFA not found for left hash " << left_hash << "." << std::endl;
      return 1;
    }

  std::string right_hash(argv[3]);
  shared_dfa_ptr right = game->load_by_hash(right_hash);
  if(right == 0)
    {
      std::cerr << "DFA not found for right hash " << right_hash << "." << std::endl;
      return 1;
    }

  UnionRestartDFA union_dfa(*left, *right);
  std::string union_name = "union_cache/" + left->get_hash() + "_" + right->get_hash() + "_restart";
  union_dfa.save(union_name);

  return 0;
}
