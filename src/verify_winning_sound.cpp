// verify_winning_sound.cpp

#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>

#include "DFAUtil.h"
#include "test_utils.h"
#include "verify_utils.h"

void verify_winning_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr losing_prev, shared_dfa_ptr won)
{
  // won probably hits less often but will be better cached
  if(won->contains(position))
    {
      // won position
      return;
    }

  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  for(const DFAString& move : moves)
    {
      if(losing_prev->contains(move))
        {
          // found winning move
          return;
        }
    }

  throw std::runtime_error("position is not won and no moves to losing positions");
}

void verify_winning_sound(const Game& game, int side_to_move, shared_dfa_ptr winning_curr, shared_dfa_ptr losing_prev)
{
  std::cout << "VERIFYING " << winning_curr->size() << " WINNING POSITIONS" << std::endl;

  shared_dfa_ptr won = game.get_positions_won(side_to_move);
  
  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(winning_curr, [&](const DFAString& position)
	{
          try
            {
              verify_winning_position(game, side_to_move, position, losing_prev, won);
            }
          catch(const std::runtime_error& e)
            {
              throw std::runtime_error(game.position_to_string(position) + "\n" + e.what());
            }
	});
    }
  catch(const std::runtime_error& e)
    {
      std::cerr << e.what() << std::endl;
      throw std::runtime_error("position not winning");
    }

  std::cout << "VERIFIED " << verified_count << " / " << winning_curr->size() << " WINNING POSITIONS" << std::endl;

  if(verified_count != uint64_t(winning_curr->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: verify_winning_sound GAME WINNING_CURR LOSING_PREV" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);
  
  std::string winning_name(argv[2]);
  std::string losing_name(argv[3]);
  std::cout << "VERIFYING " << winning_name << " WITH " << losing_name << std::endl;

  shared_dfa_ptr winning_curr = get_dfa(game_name, winning_name);
  shared_dfa_ptr losing_prev = get_dfa(game_name, losing_name);

  int side_to_move = verify_parse_side_to_move(winning_name);

  verify_winning_sound(*game, side_to_move, winning_curr, losing_prev);

  return 0;
}
