// verify_partition.cpp

#include <iostream>
#include <string>
#include <vector>

#include "game_utils.h"
#include "verify_utils.h"

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: verify_partition GAME WHOLE_DFA PART_DFA..." << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  std::string whole_name(argv[2]);
  std::vector<std::string> part_names;
  for(int i = 3; i < argc; ++i)
    {
      part_names.push_back(argv[i]);
    }

  verify_partition(*game, whole_name, part_names);

  return 0;
}
