// test_utils.cpp

#include "test_utils.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "AmazonsGame.h"
#include "BreakthroughGame.h"
#include "ChessGame.h"
#include "ClobberGame.h"
#include "DFAUtil.h"
#include "NormalNimGame.h"
#include "OthelloGame.h"
#include "TicTacToeGame.h"

bool check_loss(const Game& game, int ply_max)
{
  DFAString initial_position = game.get_position_initial();
  shared_dfa_ptr losing = game.get_positions_losing(0, ply_max);
  return losing->contains(initial_position);
}

bool check_win(const Game& game, int ply_max)
{
  DFAString initial_position = game.get_position_initial();
  shared_dfa_ptr winning = game.get_positions_winning(0, ply_max);
  return winning->contains(initial_position);
}

shared_dfa_ptr get_dfa(std::string game_name, std::string hash_or_name)
{
  const std::unique_ptr<Game> game(get_game(game_name));

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
  Game *output = 0;

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

static std::string get_tests_path(std::string game_name)
{
  return "config/" + game_name + "/tests.json";
}

static std::vector<nlohmann::json> read_test_cases(std::string game_name, std::string test_type)
{
  std::string config_path = get_tests_path(game_name);

  std::ifstream config_file(config_path);
  if(!config_file)
    {
      throw std::runtime_error(config_path + " could not be opened");
    }

  nlohmann::json config_data = nlohmann::json::parse(config_file);

  if(config_data.at("game").get<std::string>() != game_name)
    {
      throw std::runtime_error(config_path + " is for " + config_data.at("game").get<std::string>() + " instead of " + game_name);
    }

  std::vector<nlohmann::json> test_cases;
  for (auto test_case : config_data.at("tests"))
    {
      if(test_case.at("type").get<std::string>() == test_type)
        {
          test_cases.push_back(test_case);
        }
    }

  return test_cases;
}

static std::vector<std::string> get_test_game_names()
{
  std::string config_dir = "config";

  std::error_code error;
  auto dir_iter = std::filesystem::directory_iterator(config_dir, error);
  if(error)
    {
      throw std::runtime_error(config_dir + " could not be scanned: " + error.message());
    }

  std::vector<std::string> game_names;
  for(const auto& dir_entry : dir_iter)
    {
      if(!dir_entry.is_directory())
        {
          continue;
        }

      std::string game_name = dir_entry.path().filename().string();
      if(!std::filesystem::exists(get_tests_path(game_name)))
        {
          // games are not required to have tests
          continue;
        }

      game_names.push_back(game_name);
    }

  std::sort(game_names.begin(), game_names.end());

  return game_names;
}

std::vector<TestGroup> get_test_cases(std::string test_type, std::string game_name)
{
  std::vector<std::string> game_names =
    (game_name != "") ? std::vector<std::string>({game_name}) : get_test_game_names();

  std::vector<TestGroup> output;
  for(std::string current_game_name : game_names)
    {
      std::vector<nlohmann::json> test_cases = read_test_cases(current_game_name, test_type);
      if(test_cases.size() > 0)
        {
          output.emplace_back(current_game_name, test_cases);
        }
    }

  return output;
}

void run_test_cases(std::string test_type, std::string game_name, std::function<void(const Game&, const nlohmann::json&)> test_case_func)
{
  auto test_groups = get_test_cases(test_type, game_name);
  if(test_groups.size() == 0)
    {
      throw std::runtime_error("no " + test_type + " test cases found");
    }

  for(const auto& test_group : test_groups)
    {
      std::cout << "############################################################" << std::endl;
      std::cout << "GAME: " << test_group.game_name << std::endl;

      const std::unique_ptr<Game> game(get_game(test_group.game_name));

      for(const auto& test_case : test_group.test_cases)
        {
          std::cout << "############################################################" << std::endl;
          test_case_func(*game, test_case);
        }
    }
}

void test_backward(const Game& game_in, int ply_max, bool initial_win_expected)
{
  std::string log_prefix = "test_backward: ";

#if 0
  std::cout << log_prefix << "get_lost_positions()" << std::endl;

  for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
    {
      game_in.get_lost_positions(side_to_move);
    }
#endif

  auto initial_positions = game_in.get_positions_initial();

  // first player should never lose via strategy stealing argument.

  std::cout << log_prefix << "get_positions_winning()" << std::endl;
  auto winning_positions = game_in.get_positions_winning(0, ply_max);
  auto initial_winning = DFAUtil::get_intersection(initial_positions, winning_positions);
  if(initial_win_expected)
    {
      assert(initial_winning->size() > 0);
    }
  else
    {
      // draws with perfect play
      assert(initial_winning->size() == 0);
      std::cout << "  rejected win" << std::endl;
    }
}

void test_forward(const Game& game_in, const std::vector<size_t>& positions_expected)
{
  assert(positions_expected.size() > 0);

  std::string log_prefix = "test_forward: ";

  std::cout << log_prefix << "get_positions_initial()" << std::endl;

  auto initial_positions = game_in.get_positions_initial();
  assert(initial_positions);

  std::cout << game_in.position_to_string(game_in.get_position_initial()) << std::endl;
  assert(size_t(initial_positions->size()) == positions_expected[0]);

  std::cout << log_prefix << "get_moves_forward()" << std::endl;

  auto current_positions = initial_positions;
  for(int depth = 0; depth + 1 < positions_expected.size(); ++depth)
    {
      int side_to_move = depth % 2;
      current_positions = game_in.get_moves_forward(side_to_move, current_positions);
      std::cout << log_prefix << "depth " << (depth + 1) << ": " << current_positions->states() << " states, " << current_positions->size() << " positions" << std::endl;

      assert(size_t(current_positions->size()) == positions_expected.at(depth + 1));
    }
}

void test_game(const Game& game_in, const std::vector<size_t>& positions_expected, int ply_max, bool initial_win_expected)
{
  test_forward(game_in, positions_expected);
  test_backward(game_in, ply_max, initial_win_expected);
}
