// test_terminal.cpp

// Checks each game's terminal sets against its DFA move generator, for each
// side to move:
//
// * won and lost are disjoint;
// * no position in won has a move;
// * no position in lost has a move.
//
// The second and third check the forward move generator against terminal
// sets built separately (for normal-play games, from the backward moves of
// every position). Only DFA operations are used, so this tests whether
// separately built DFAs agree, not the rules; the verify_* programs check
// positions against the rules. It does not check that every position with
// no moves is in won or lost, or tied; see TODO.md.
//
// Runs on an explicit list of small sizes. Bigger sizes are left out because
// their lost sets and the forward moves from them are too big for make test,
// not because they pass.

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "DFAUtil.h"
#include "game_utils.h"

static void check_empty(const std::string& game_name, int side_to_move, const std::string& what, shared_dfa_ptr dfa)
{
  if(!dfa->is_constant(0))
    {
      throw std::runtime_error(game_name + " side " + std::to_string(side_to_move) + ": " + what + " holds " + std::to_string(dfa->size()) + " positions, expected none");
    }
  std::cout << "  " << what << ": empty" << std::endl;
}

static void test_terminal(const std::string& game_name)
{
  Game *game = get_game(game_name);

  for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
    {
      shared_dfa_ptr won = game->get_positions_won(side_to_move);
      shared_dfa_ptr lost = game->get_positions_lost(side_to_move);
      std::cout << game_name << " side " << side_to_move << ": " << won->size() << " won, " << lost->size() << " lost positions" << std::endl;

      check_empty(game_name, side_to_move, "won and lost intersection", DFAUtil::get_intersection(won, lost));
      check_empty(game_name, side_to_move, "moves from won positions", game->get_moves_forward(side_to_move, won));
      check_empty(game_name, side_to_move, "moves from lost positions", game->get_moves_forward(side_to_move, lost));
    }
}

int main()
{
  std::vector<std::string> games = {
    "tictactoe_2",
    "tictactoe_3",
    "tictactoe_4",
    "normalnim_3x3",
    "amazons_4x4",
    "breakthrough_4x4",
    "clobber_2x2",
    "clobber_4x4",
    "cram_2x2",
    "cram_4x4"
  };

  std::cout << "TESTING TERMINAL SETS FOR";
  for(const std::string& game_name : games)
    {
      std::cout << " " << game_name;
    }
  std::cout << std::endl;
  std::cout << "NOT TESTED: slidingtile (one-player puzzles, no won or lost sets), chess (has_moves not built yet), and every bigger size of the games above" << std::endl;

  for(const std::string& game_name : games)
    {
      test_terminal(game_name);
    }

  std::cout << "TERMINAL SETS PASSED FOR " << games.size() << " GAMES" << std::endl;

  return 0;
}
