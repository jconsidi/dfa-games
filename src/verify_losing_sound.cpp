// verify_losing_sound.cpp

#include <iostream>

#include "test_utils.h"
#include "verify_utils.h"

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: verify_losing_sound GAME LOSING_CURR WINNING_PREV" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);
  
  std::string losing_name(argv[2]);
  std::string winning_name(argv[3]);
  std::cout << "VERIFYING " << losing_name << " WITH " << winning_name << std::endl;

  shared_dfa_ptr losing_curr = get_dfa(game_name, losing_name);
  shared_dfa_ptr winning_prev = get_dfa(game_name, winning_name);

  int side_to_move = verify_parse_side_to_move(losing_name);

  verify_losing_sound(*game, side_to_move, losing_curr, winning_prev);

  return 0;
}
