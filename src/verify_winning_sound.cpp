// verify_winning_sound.cpp

#include <iostream>

#include "test_utils.h"
#include "verify_utils.h"

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: verify_winning_sound GAME WINNING_CURR LOSING_PREV" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);
  
  std::string winning_name(argv[2]);
  std::string losing_name(argv[3]);
  std::cout << "VERIFYING " << winning_name << " WITH " << losing_name << std::endl;

  shared_dfa_ptr winning_curr = get_dfa(game_name, winning_name);
  shared_dfa_ptr losing_prev = get_dfa(game_name, losing_name);

  int side_to_move = verify_parse_side_to_move(winning_name);

  verify_winning_sound(*game, side_to_move, winning_curr, losing_prev);

  return 0;
}
