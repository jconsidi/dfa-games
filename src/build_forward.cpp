// test_forward.cpp

#include <cstdlib>
#include <iostream>

#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_forward GAME_NAME [depth]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = (argc >= 3) ? atoi(argv[2]) : 2;

  auto initial_positions = game->get_positions_initial();
  assert(initial_positions->size() == 1);

  for(auto iter = initial_positions->cbegin();
      iter < initial_positions->cend();
      ++iter)
    {
      std::cout << game->position_to_string(*iter) << std::endl;
    }

  for(int ply = 0; ply <= ply_max; ++ply)
    {
      shared_dfa_ptr positions = game->get_positions_forward(ply);
      std::cout << positions->size() << " positions after " << ply << " ply." << std::endl;
      if(positions->size() == 0)
	{
	  break;
	}
    }

  return 0;
}
