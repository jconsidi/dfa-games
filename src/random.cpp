// random.cpp

#include <chrono>
#include <iostream>
#include <random>

#include "utils.h"

int main(int argc, char **argv)
{
  const char *fen = (argc > 1) ? argv[1] : INITIAL_FEN;
  Board board(fen);
  std::cout << board << std::endl;

  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::default_random_engine generator(seed);
  for(int half_move = 0; !board.is_final(); ++half_move)
    {
      Board moves[CHESS_MAX_MOVES];
      int num_moves = board.generate_moves(moves);
      if(num_moves == 0)
	{
	  break;
	}

      std::uniform_int_distribution<int> distribution(0, num_moves - 1);
      board = moves[distribution(generator)];

      std::cout << "HALF MOVE " << half_move << std::endl;
      std::cout << board << std::endl;
    }

  std::string verdict;
  if(board.is_checkmate())
    {
      verdict = "CHECKMATE";
    }
  else if(board.is_stalemate())
    {
      verdict = "STALEMATE";
    }
  else if(board.is_draw_by_rule())
    {
      verdict = "DRAW by 50 move rule";
    }
  else if(board.is_draw_by_material())
    {
      verdict = "DRAW by material";
    }
  else
    {
      verdict = "UNKNOWN";
    }

  std::cout << verdict << std::endl;

  return 0;
}
