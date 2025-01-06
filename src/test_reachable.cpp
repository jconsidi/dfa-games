// test_reachable.cpp

#include <cstdlib>
#include <iostream>

#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_reachable GAME_NAME [side_to_move] [ply]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int side_to_move = (argc >= 3) ? atoi(argv[2]) : 0;

  int ply = (argc >= 4) ? atoi(argv[3]) : 2;

  shared_dfa_ptr reachable = game->get_positions_reachable(side_to_move, ply);
  auto positions = reachable->size();
  auto states = reachable->states();
  std::cout << "ply " << ply << ": " << positions << " positions, " << states << " states, " << (positions / double(states)) << " positions/state" << std::endl;

  return 0;
}
