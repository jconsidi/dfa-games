// stats_backward.cpp

#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  if(argc < 2)
    {
      std::cerr << "usage: stats_backward GAME_NAME [BACKWARD_PLY_MAX]";
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int backward_ply_max = (argc >= 3) ? atoi(argv[2]) : 1000;

  std::vector<std::string> result_order = {"winning", "losing", "unknown"};

  std::cout << "ply";
  for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
    {
      for(std::string result : result_order)
        {
          std::cout << std::format("\t{0:s}_states_{1:d}\t{0:s}_positions_{1:d}", result, side_to_move);
        }
    }
  std::cout << std::endl;

  for(int ply = 0; ply <= backward_ply_max; ++ply)
    {
      std::ostringstream row_output;
      row_output << ply;

      for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
        {
          for(std::string result : result_order)
            {
              std::string dfa_name = std::format("backward,ply_max={:03d},side={:d},{:s}", ply, side_to_move, result);

              shared_dfa_ptr dfa = game->load_by_name(dfa_name);
              if(!dfa)
                {
                  std::cerr << "DFA " << dfa_name << " not found." << std::endl;
                  return 1;
                }

              row_output << "\t" << dfa->states() << "\t" << dfa->size();
            }
        }

      std::cout << row_output.str() << std::endl;
    }

  return 0;
}
