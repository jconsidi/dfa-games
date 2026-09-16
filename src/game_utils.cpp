// game_utils.cpp

#include "game_utils.h"

#include <charconv>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "AmazonsGame.h"
#include "BreakthroughGame.h"
#include "ChessGame.h"
#include "ClobberGame.h"
#include "CramGame.h"
#include "NormalNimGame.h"
#include "OthelloGame.h"
#include "SlidingTilePuzzle.h"
#include "TicTacToeGame.h"

static int parse_int(const std::string& game_name, std::string_view field)
{
  int value = 0;
  auto result = std::from_chars(field.data(), field.data() + field.size(), value);
  if((result.ec != std::errc()) || (result.ptr != field.data() + field.size()))
    {
      throw std::logic_error("could not parse game name \"" + game_name + "\"");
    }

  return value;
}

// Parses the part of game_name after prefix as "<int>x<int>", requiring
// every remaining character to be consumed. sscanf's "%dx%d" would accept
// trailing garbage like "breakthrough_4x4xyz" silently, leaving that case
// to be caught later (if at all) by the get_name() cross-check below with
// a message that doesn't say what was actually wrong.
static void parse_dims(const std::string& game_name, std::string_view prefix, int& width, int& height)
{
  std::string_view rest(game_name);
  rest.remove_prefix(prefix.size());

  size_t x_pos = rest.find('x');
  if(x_pos == std::string_view::npos)
    {
      throw std::logic_error("could not parse game name \"" + game_name + "\"");
    }

  width = parse_int(game_name, rest.substr(0, x_pos));
  height = parse_int(game_name, rest.substr(x_pos + 1));
}

static int parse_int_after(const std::string& game_name, std::string_view prefix)
{
  std::string_view rest(game_name);
  rest.remove_prefix(prefix.size());
  return parse_int(game_name, rest);
}

shared_dfa_ptr get_dfa(std::string game_name, std::string hash_or_name)
{
  const std::unique_ptr<GameBase> game(get_gamebase(game_name));

  if(hash_or_name.length() == 64)
    {
      shared_dfa_ptr hash_dfa = game->load_by_hash(hash_or_name);
      if(hash_dfa)
	{
	  return hash_dfa;
	}
    }

  // Callers dereference what they get back without checking, and the load
  // path only reports a missing file as "open() failed" with no indication
  // of which DFA was wanted.

  shared_dfa_ptr output;
  try
    {
      output = game->load(hash_or_name);
    }
  catch(const std::runtime_error& e)
    {
      throw std::runtime_error("could not load DFA \"" + hash_or_name +
			       "\" for game \"" + game_name + "\": " + e.what());
    }

  if(!output)
    {
      throw std::runtime_error("no DFA named \"" + hash_or_name +
			       "\" for game \"" + game_name + "\"");
    }

  return output;
}

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
      parse_dims(game_name, "amazons_", width, height);
      output = new AmazonsGame(width, height);
    }
  else if(game_name.starts_with("breakthrough_"))
    {
      int width = 0;
      int height = 0;
      parse_dims(game_name, "breakthrough_", width, height);
      output = new BreakthroughGame(width, height);
    }
  else if(game_name.starts_with("breakthroughcw_"))
    {
      int width = 0;
      int height = 0;
      parse_dims(game_name, "breakthroughcw_", width, height);
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
      parse_dims(game_name, "clobber_", width, height);
      output = new ClobberGame(width, height);
    }
  else if(game_name.starts_with("cram_"))
    {
      int width = 0;
      int height = 0;
      parse_dims(game_name, "cram_", width, height);
      output = new CramGame(width, height);
    }
  else if(game_name.starts_with("normalnim_"))
    {
      int num_heaps = 0;
      int heap_max = 0;
      parse_dims(game_name, "normalnim_", num_heaps, heap_max);
      output = new NormalNimGame(num_heaps, heap_max);
    }
  else if(game_name.starts_with("othello_"))
    {
      int width = 0;
      int height = 0;
      parse_dims(game_name, "othello_", width, height);
      output = new OthelloGame(width, height);
    }
  else if(game_name.starts_with("slidingtile_"))
    {
      int width = 0;
      int height = 0;
      parse_dims(game_name, "slidingtile_", width, height);
      output = new SlidingTilePuzzle(width, height);
    }
  else if(game_name.starts_with("tictactoe_"))
    {
      output = new TicTacToeGame(parse_int_after(game_name, "tictactoe_"));
    }
  else
    {
      throw std::logic_error("get_gamebase() did not recognize game name \"" + game_name + "\"");
    }

  assert(output->get_name() == game_name);
  return output;
}
