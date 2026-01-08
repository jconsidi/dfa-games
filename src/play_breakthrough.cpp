// play-breakthrough.cpp

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "DFA.h"
#include "Game.h"
#include "utils.h"
#include "test_utils.h"

int main(int argc, char **argv)
{
  int ply = (argc > 1) ? atoi(argv[1]) : 0;
  const char *fen = (argc > 2) ? argv[2] : "bbbbbb/bbbbbb/....../....../wwwwww/wwwwww w";

  if(strlen(fen) != 36 + 5 + 2)
    {
      std::cerr << "FEN string has wrong length." << std::endl;
      return 1;
    }

  Game *game = get_game("breakthrough_6x6");
  std::vector<int> fen_vector;
  for(int row = 0; row < 6; ++row)
    {
      for(int col = 0; col < 6; ++col)
        {
          char c = fen[col + row * 7];
          if(c == '.')
            {
              fen_vector.push_back(0);
            }
          else if(c == 'w')
            {
              fen_vector.push_back(1);
            }
          else if (c == 'b')
            {
              fen_vector.push_back(2);
            }
          else
            {
              std::cerr << "invalid FEN character " << c << std::endl;
              return 1;
            }
        }
    }
  // reverse
  for(int i = 0; i < 18; ++i)
    {
      int c = fen_vector[i];
      fen_vector[i] = fen_vector[35 - i];
      fen_vector[35 - i] = c;
    }

  DFAString position = DFAString(game->get_shape(), fen_vector);
  std::cout << game->position_to_string(position) << std::endl;

  shared_dfa_ptr reachable = game->get_positions_forward(ply);
  if(!reachable->contains(position))
    {
      std::cerr << "Position is not reachable in exactly " << ply << " ply." << std::endl;
      return 1;
    }

  std::ostringstream winning_name_builder;
  winning_name_builder << "forward_backward,forward_ply_max=085,backward_ply_max=000,ply=" << std::setfill('0') << std::setw(3) << ply << ",winning";
  std::string winning_name = winning_name_builder.str();
  std::cout << "WINNING " << winning_name << std::endl;

  shared_dfa_ptr winning = game->load(winning_name);
  if(!winning->contains(position))
    {
      std::cerr << "Position is not winning." << std::endl;
      return 1;
    }

  std::ostringstream losing_name_builder;
  losing_name_builder << "forward_backward,forward_ply_max=085,backward_ply_max=000,ply=" << std::setfill('0') << std::setw(3) << (ply+1) << ",losing";
  std::string losing_name = losing_name_builder.str();
  std::cout << "LOSING " << losing_name << std::endl;

  shared_dfa_ptr losing = game->load(losing_name);
  for(auto move : game->validate_moves(ply % 2, position))
    {
      if(losing->contains(move))
        {
          std::cout << "WINNING MOVE:" << std::endl;
          std::cout << game->position_to_string(move) << std::endl;
          return 0;
        }
    }

  std::cerr << "WINNING MOVE NOT FOUND." << std::endl;
  return 1;
}
