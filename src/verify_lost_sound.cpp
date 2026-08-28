// verify_lost_sound.cpp

#include <iostream>

#include "test_utils.h"
#include "verify_utils.h"

int main(int argc, char **argv)
{
  if(argc < 3)
    {
      std::cerr << "usage: verify_lost_sound GAME DFA_NAME" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);
  
  std::string name(argv[2]);
  std::cout << "VERIFYING " << name << std::endl;
  shared_dfa_ptr positions = get_dfa(game_name, name);

  int side_to_move = verify_parse_side_to_move(name);

  verify_lost_sound(*game, side_to_move, positions);

  return 0;
}
