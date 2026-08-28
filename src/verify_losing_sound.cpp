// verify_losing_sound.cpp

#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>

#include "DFAUtil.h"
#include "test_utils.h"
#include "verify_utils.h"

void verify_losing_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr winning_prev, shared_dfa_ptr lost)
{
  // lost probably hits less often but will be better cached
  if(lost->contains(position))
    {
      // lost position
      return;
    }

  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  for(const DFAString& move : moves)
    {
      if(!winning_prev->contains(move))
        {
          // found move that may not lose
          throw std::runtime_error("position is not lost and has moves to not known losing positions");
        }
    }
}

void verify_losing_sound(const Game& game, int side_to_move, shared_dfa_ptr losing_curr, shared_dfa_ptr winning_prev)
{
  std::cout << "VERIFYING " << losing_curr->size() << " LOSING POSITIONS" << std::endl;

  shared_dfa_ptr lost = game.get_positions_lost(side_to_move);
  
  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(losing_curr, [&](const DFAString& position)
	{
          try
            {
              verify_losing_position(game, side_to_move, position, winning_prev, lost);
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
      throw std::runtime_error("position not losing");
    }

  std::cout << "VERIFIED " << verified_count << " / " << losing_curr->size() << " LOSING POSITIONS" << std::endl;

  if(verified_count != uint64_t(losing_curr->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}

int main(int argc, char **argv)
{
  if(argc < 4)
    {
      std::cerr << "usage: verify_losing_sound GAME LOSING_CURR WINNING_PREV" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);
  
  std::string losing_name(argv[2]);
  std::string winning_name(argv[3]);
  std::cout << "VERIFYING " << losing_name << " WITH " << winning_name << std::endl;

  shared_dfa_ptr losing_curr = get_dfa(game_name, losing_name);
  shared_dfa_ptr winning_prev = get_dfa(game_name, winning_name);

  int side_to_move = verify_parse_side_to_move(losing_name);

  verify_losing_sound(*game, side_to_move, losing_curr, winning_prev);

  return 0;
}
