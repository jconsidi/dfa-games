// verify_won_sound.cpp

#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "DFAUtil.h"
#include "test_utils.h"
#include "verify_utils.h"

void verify_won_position(const Game& game, int side_to_move, const DFAString& position)
{
  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  bool moves_mismatch = moves.size() > 0;

  std::optional<int> result_actual = game.validate_result(side_to_move, position);
  bool result_mismatch = !result_actual || (*result_actual != 1);

  if(result_mismatch || moves_mismatch)
    {
      // built up instead of printed so that the caller does the writing.
      // for_each_position may run this on many positions at once, and
      // interleaved reports would be unreadable.

      std::ostringstream report;

      report << game.position_to_string(position) << "\n";

      if(!result_actual)
        {
          report << "# RESULT MISMATCH: not terminal" << "\n";
        }
      else if(result_mismatch)
        {
          report << "# RESULT MISMATCH: expected won (1), actual " << *result_actual << "\n";
        }

      if(moves_mismatch)
        {
          report << "# MOVES MISMATCH: expected some, actual " << moves.size() << "\n";
        }

      report << "\n";

      throw std::runtime_error(report.str());
    }
}

void verify_won_sound(const Game& game, int side_to_move, shared_dfa_ptr positions)
{
  std::cout << "VERIFYING " << positions->size() << " WON POSITIONS" << std::endl;
  
  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(positions, [&](const DFAString& position)
	{
	  verify_won_position(game, side_to_move, position);
	});
    }
  catch(const std::runtime_error& e)
    {
      std::cerr << e.what() << std::endl;
      throw std::runtime_error("position not won");
    }

  std::cout << "VERIFIED " << verified_count << " / " << positions->size() << " WON POSITIONS" << std::endl;

  if(verified_count != uint64_t(positions->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}

int main(int argc, char **argv)
{
  if(argc < 3)
    {
      std::cerr << "usage: verify_won_sound GAME DFA_NAME" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);
  
  std::string name(argv[2]);
  std::cout << "VERIFYING " << name << std::endl;
  shared_dfa_ptr positions = get_dfa(game_name, name);

  int side_to_move = verify_parse_side_to_move(name);

  verify_won_sound(*game, side_to_move, positions);

  return 0;
}
