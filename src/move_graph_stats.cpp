// move_graph_stats.cpp

#include <iostream>

#include "DFA.h"
#include "Game.h"
#include "utils.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc != 2)
    {
      std::cerr << "usage: " << argv[0] << " GAME" << std::endl;
      return 1;
    }

  // args = game, DFA name
  std::string game_name(argv[1]);

  Game *game = get_game(game_name);
  const MoveGraph& move_graph = game->get_move_graph_forward(0);

  std::cout << "move graph size = " << move_graph.size() << std::endl;

  return 0;
}
