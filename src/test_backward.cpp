// test_backward.cpp

#include <iostream>
#include <map>
#include <string>

#include "test_utils.h"

static std::map<std::string, std::pair<int, int>> solved_games;

void populate_solved_games()
{
  solved_games.try_emplace("amazons_4x4", 8, -1);
  solved_games.try_emplace("amazons_4x5", 12, 1); /* Song 2014 */
  solved_games.try_emplace("amazons_4x6", 16, 1); /* Song 2014 */
  solved_games.try_emplace("amazons_4x7", 20, 1); /* Song 2014 */
  solved_games.try_emplace("amazons_5x4", 12, 1); /* Song 2014 */
  solved_games.try_emplace("amazons_5x5", 17, 1); /* Muller 2001 */
  solved_games.try_emplace("amazons_5x6", 22, 1); /* Song 2014 */
  solved_games.try_emplace("amazons_6x4", 16, -1); /* Song 2014 */
  solved_games.try_emplace("tictactoe_1", 1, 1);
  solved_games.try_emplace("tictactoe_2", 3, 1);
  solved_games.try_emplace("tictactoe_3", 9, 0);
  solved_games.try_emplace("tictactoe_4", 16, 0);
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: test_backward GAME_NAME\n";
      return 1;
    }

  populate_solved_games();

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  DFAString initial_position = game->get_position_initial();
  std::cout << "INITIAL POSITION:" << std::endl;
  std::cout << game->position_to_string(initial_position) << std::endl;

  // test based on whether game has known solution.

  auto search = solved_games.find(game_name);
  if(search != solved_games.end())
    {
      int ply_max = std::get<0>(search->second);
      int result_expected = std::get<1>(search->second);

      std::cout << "PLY MAX = " << ply_max << std::endl;
      std::cout << "RESULT EXPECTED = " << result_expected << std::endl;

      if(result_expected > 0)
	{
	  /* win expected */
	  assert(check_win(*game, ply_max));
	}
      else if(result_expected < 0)
	{
	  /* loss expected */
	  assert(check_loss(*game, ply_max));
	}
      else
	{
	  /* draw expected */
	  assert(!check_win(*game, ply_max));
	  assert(!check_loss(*game, ply_max));
	}
    }
  else
    {
      shared_dfa_ptr winning_positions = game->get_positions_winning(0, 10);
    }

  return 0;
}
