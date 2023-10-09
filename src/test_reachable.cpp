// test_reachable.cpp

#include <cstdlib>
#include <iostream>

#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_reachable GAME_NAME [depth]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply = (argc >= 3) ? atoi(argv[2]) : 2;

  shared_dfa_ptr reachable = game->get_positions_reachable(ply);
  auto positions = reachable->size();
  auto states = reachable->states();
  std::cout << "ply " << ply << ": " << positions << " positions, " << states << " states, " << (positions / states) << " positions/state" << std::endl;

  return 0;
}
