// test_get_intersection.cpp

#include <cstdlib>
#include <iostream>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: test_get_intersection GAME_NAME LEFT_HASH RIGHT_HASH\n";
      return 1;
    }

  std::string game_name(argv[1]);

  std::string left_hash_or_name(argv[2]);
  shared_dfa_ptr left = get_dfa(game_name, left_hash_or_name);
  if(left == 0)
    {
      std::cerr << "DFA not found for left hash or name " << left_hash_or_name << "." << std::endl;
      return 1;
    }
  
  std::string right_hash_or_name(argv[3]);
  shared_dfa_ptr right = get_dfa(game_name, right_hash_or_name);
  if(right == 0)
    {
      std::cerr << "DFA not found for right hash or name " << right_hash_or_name << "." << std::endl;
      return 1;
    }

  shared_dfa_ptr test_intersection = DFAUtil::get_intersection(left, right);
  assert(test_intersection != 0);
  
  return 0;
}
