// test_utils.cpp

#include "test_utils.h"

#include <cstdio>
#include <iostream>

#include "AmazonsGame.h"
#include "ChessGame.h"
#include "DFAUtil.h"
#include "NormalNimGame.h"
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
#if CHESS_SQUARE_OFFSET == 0
  else if(game_name == "chess+0")
    {
      output = new ChessGame();
    }
#elif CHESS_SQUARE_OFFSET == 2
  else if(game_name == "chess+2")
    {
      output = new ChessGame();
    }
#endif
  else if(game_name.starts_with("normalnim_"))
    {
      int heaps = 0;
      if(std::sscanf(game_name.c_str(), "normalnim_%d", &heaps) != 1)
	{
	  throw std::logic_error("get_name() failed parsing normalnim game name");
	}

      output = new NormalNimGame(dfa_shape_t(heaps, 16));
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

  for(auto iter = initial_positions->cbegin();
      iter < initial_positions->cend();
      ++iter)
    {
      std::cout << game_in.position_to_string(*iter) << std::endl;
    }
  assert(initial_positions->size() == positions_expected[0]);

  std::cout << log_prefix << "get_moves_forward()" << std::endl;

  auto current_positions = initial_positions;
  for(int depth = 0; depth + 1 < positions_expected.size(); ++depth)
    {
      int side_to_move = depth % 2;
      current_positions = game_in.get_moves_forward(side_to_move, current_positions);
      std::cout << log_prefix << "depth " << (depth + 1) << ": " << current_positions->states() << " states, " << current_positions->size() << " positions" << std::endl;

      assert(current_positions->size() == positions_expected.at(depth + 1));
    }
}

void test_game(const Game& game_in, const std::vector<size_t>& positions_expected, int ply_max, bool initial_win_expected)
{
  test_forward(game_in, positions_expected);
  test_backward(game_in, ply_max, initial_win_expected);
}
