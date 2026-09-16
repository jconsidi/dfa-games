// game_utils.cpp

#include "game_utils.h"

#include <cstdio>
#include <stdexcept>

#include "AmazonsGame.h"
#include "BreakthroughGame.h"
#include "ChessGame.h"
#include "ClobberGame.h"
#include "CramGame.h"
#include "NormalNimGame.h"
#include "OthelloGame.h"
#include "SlidingTilePuzzle.h"
#include "TicTacToeGame.h"

Game *get_game(std::string game_name)
{
  GameBase *output = get_gamebase(game_name);
  return dynamic_cast<Game *>(output);
}

GameBase *get_gamebase(std::string game_name)
{
  GameBase *output = 0;

  if(game_name.starts_with("amazons_"))
    {
      int width = 0;
      int height = 0;
      if(std::sscanf(game_name.c_str(), "amazons_%dx%d", &width, &height) != 2)
	{
	  throw std::logic_error("get_name() failed parsing amazons game name");
	}
      output = new AmazonsGame(width, height);
    }
  else if(game_name.starts_with("breakthrough_"))
    {
      int width = 0;
      int height = 0;
      if(std::sscanf(game_name.c_str(), "breakthrough_%dx%d", &width, &height) != 2)
	{
	  throw std::logic_error("get_name() failed parsing breakthrough game name");
	}
      output = new BreakthroughGame(width, height);
    }
  else if(game_name.starts_with("breakthroughcw_"))
    {
      int width = 0;
      int height = 0;
      if(std::sscanf(game_name.c_str(), "breakthroughcw_%dx%d", &width, &height) != 2)
	{
	  throw std::logic_error("get_name() failed parsing breakthroughcw game name");
	}
      output = new BreakthroughColumnWiseGame(width, height);
    }
#if CHESS_SQUARE_OFFSET == 0
  else if(game_name == "chess+0")
    {
      output = new ChessGame();
    }
#elif CHESS_SQUARE_OFFSET == 1
  else if(game_name == "chess+1")
    {
      output = new ChessGame();
    }
#elif CHESS_SQUARE_OFFSET == 2
  else if(game_name.starts_with("chess+2"))
    {
      output = new ChessGame();
    }
#endif
  else if(game_name.starts_with("clobber_"))
    {
      int width = 0;
      int height = 0;
      if(std::sscanf(game_name.c_str(), "clobber_%dx%d", &width, &height) != 2)
	{
	  throw std::logic_error("get_name() failed parsing clobber game name");
	}
      output = new ClobberGame(width, height);
    }
  else if(game_name.starts_with("cram_"))
    {
      int width = 0;
      int height = 0;
      if(std::sscanf(game_name.c_str(), "cram_%dx%d", &width, &height) != 2)
	{
	  throw std::logic_error("get_name() failed parsing cram game name");
	}
      output = new CramGame(width, height);
    }
  else if(game_name.starts_with("normalnim_"))
    {
      int num_heaps = 0;
      int heap_max = 0;
      if(std::sscanf(game_name.c_str(), "normalnim_%dx%d", &num_heaps, &heap_max) != 2)
	{
	  throw std::logic_error("get_name() failed parsing normalnim game name");
	}

      output = new NormalNimGame(num_heaps, heap_max);
    }
  else if(game_name.starts_with("othello_"))
    {
      int width = 0;
      int height = 0;
      if(std::sscanf(game_name.c_str(), "othello_%dx%d", &width, &height) != 2)
	{
	  throw std::logic_error("get_name() failed parsing othello game name");
	}
      output = new OthelloGame(width, height);
    }
  else if(game_name.starts_with("slidingtile_"))
    {
      int width = 0;
      int height = 0;
      if(std::sscanf(game_name.c_str(), "slidingtile_%dx%d", &width, &height) != 2)
	{
	  throw std::logic_error("get_name() failed parsing slidingtile game name");
	}
      output = new SlidingTilePuzzle(width, height);
    }
  else if(game_name.starts_with("tictactoe_"))
    {
      int n = 0;
      if(std::sscanf(game_name.c_str(), "tictactoe_%d", &n) != 1)
	{
	  throw std::logic_error("get_name() failed parsing tictactoe game name");
	}

      output = new TicTacToeGame(n);
    }
  else
    {
      throw std::logic_error("get_name() did not recognize game name");
    }

  assert(output->get_name() == game_name);
  return output;
}
