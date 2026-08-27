// verify_won_sound.cpp

#include <iostream>
#include <optional>

#include "test_utils.h"
#include "verify_utils.h"

void verify_lost_position(const Game& game, int side_to_move, const DFAString& position)
{
  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  bool moves_mismatch = moves.size() > 0;

  std::optional<int> result_actual = game.validate_result(side_to_move, position);
  bool result_mismatch = !result_actual || (*result_actual != -1);

  if(result_mismatch || moves_mismatch)
    {
      std::cerr << game.position_to_string(position) << std::endl;

      if(!result_actual)
        {
          std::cerr << "# RESULT MISMATCH: not terminal" << std::endl;
        }
      else if(result_mismatch)
        {
          std::cerr << "# RESULT MISMATCH: expected lost (-1), actual " << *result_actual << std::endl;
        }

      if(moves_mismatch)
        {
          std::cerr << "# MOVES MISMATCH: expected some, actual " << moves.size() << std::endl;
        }

      std::cerr << std::endl;

      throw std::runtime_error("position not lost");
    }
}

void verify_lost_sound(const Game& game, int side_to_move, shared_dfa_ptr positions)
{
  std::cout << "VERIFYING " << positions->size() << " POSITIONS" << std::endl;
  
  uint64_t verified_count = 0;
  for(auto iter = positions->cbegin();
      iter < positions->cend();
      ++iter, ++verified_count)
    {
      DFAString position(*iter);
      verify_lost_position(game, side_to_move, position);
    }

  std::cout << "VERIFIED " << verified_count << " / " << positions->size() << " POSITIONS" << std::endl;

  if(verified_count != uint64_t(positions->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}

int main(int argc, char **argv)
{
  if(argc < 3)
    {
      std::cerr << "usage: verify_lost_sound GAME DFA_NAME" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);
  
  std::string name(argv[2]);
  shared_dfa_ptr positions = get_dfa(game_name, name);

  int side_to_move = verify_parse_side_to_move(name);

  verify_lost_sound(*game, side_to_move, positions);

  return 0;
}
