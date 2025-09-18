// build_forward_backward_proof.cpp

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"
#include "parallel.h"
#include "test_utils.h"

std::string get_name(int forward_ply_max, int backward_ply_max, int ply, std::string result)
{
  std::ostringstream output;
  output << "forward_backward";
  output << ",forward_ply_max=" << std::setfill('0') << std::setw(3) << forward_ply_max;
  output << ",backward_ply_max=" << std::setfill('0') << std::setw(3) << backward_ply_max;
  output << ",ply=" << std::setfill('0') << std::setw(3) << ply;
  output << "," << result;

  return output.str();
}

shared_dfa_ptr get_positions(const Game& game, int forward_ply_max, int backward_ply_max, int ply, std::string result)
{
  std::string name = get_name(forward_ply_max, backward_ply_max, ply, result);
  std::cout << "LOADING " << name << std::endl;

  shared_dfa_ptr dfa = game.load(name);
  if(!dfa)
    {
      throw std::logic_error("failed loading " + name);
    }
  return dfa;
}

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: build_forward_backward_proof GAME_NAME [FORWARD_PLY] [BACKWARD_PLY]\n";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int forward_ply_max = (argc >= 3) ? atoi(argv[2]) : 100;
  int backward_ply_max = (argc >= 4) ? atoi(argv[3]) : 0;

  auto initial_positions = game->get_positions_initial();
  assert(initial_positions->size() == 1);

  DFAString initial_position = *(initial_positions->cbegin());

  shared_dfa_ptr winning_0 = get_positions(*game, forward_ply_max, backward_ply_max, 0, "winning");
  bool initial_winning = winning_0->contains(initial_position);

  shared_dfa_ptr curr_positions = initial_positions;
  for(int ply = 0; ply <= forward_ply_max; ++ply)
    {
      int side_to_move = ply % 2;
      bool ply_winning = initial_winning ^ (ply % 2);
      
      std::cout << "PLY " << ply << " " << (ply_winning ? "WINNING" : "LOSING") << " " << curr_positions->size() << " positions to prove." << std::endl;

      // load next losing positions if proving wins now.
      shared_dfa_ptr next_losing = ply_winning ? get_positions(*game, forward_ply_max, backward_ply_max, ply + 1, "losing") : 0;
      
      if(ply_winning)
        {
          std::vector<DFAString> next_positions;

          for(auto iter = curr_positions->cbegin();
              iter < curr_positions->cend();
              ++iter)
            {
              DFAString curr_position = *iter;

              std::vector<DFAString> temp_positions = game->validate_moves(side_to_move, curr_position);
              // must have at least one move to a losing position
              assert(temp_positions.size() > 0);
              bool found_loss = false;
              for(const DFAString &temp_position : temp_positions)
                {
                  if(next_losing->contains(temp_position))
                    {
                      next_positions.push_back(temp_position);
                      found_loss = true;
                      break;
                    }
                }
              assert(found_loss);
            }

          curr_positions = DFAUtil::from_strings(game->get_shape(), next_positions);
        }
      else // ply is losing
        {
          // all moves must go to winning positions
          curr_positions = game->get_moves_forward(side_to_move, curr_positions);
        }

      std::cout << "  compressed to " << curr_positions->states() << " states" << std::endl;
      auto size = curr_positions->size();
      std::cout << "  compressed " << size << " positions" << std::endl;
    }

  return 0;
}
