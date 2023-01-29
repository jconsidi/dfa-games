// print.cpp

#include <iostream>

#include "DFA.h"
#include "Game.h"
#include "chess.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc == 2)
    {
      // assume chess FEN string
      const char *fen = (argc > 1) ? argv[1] : INITIAL_FEN;
      Board board(fen);
      std::cout << board;
    }
  else if(argc == 3)
    {
      // args = game, DFA name
      std::string game_name(argv[1]);
      Game *game = get_game(game_name);

      std::string positions_name(argv[2]);
      shared_dfa_ptr positions = game->load(positions_name);

      for(auto iter = positions->cbegin();
	  iter < positions->cend();
	  ++iter)
	{
	  DFAString position_string = *iter;
	  std::cout << game->position_to_string(position_string) << std::endl;
	  std::cout << std::endl;
	}
    }

  return 0;
}
