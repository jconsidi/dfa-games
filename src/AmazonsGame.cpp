// AmazonsGame.cpp

#include "AmazonsGame.h"

#include <cstdlib>
#include <format>
#include <sstream>

#include "DFAUtil.h"
#include "GameUtil.h"

static std::string build_name(int width, int height)
{
  return std::format("amazons_{:d}x{:d}", width, height);
}

static std::vector<std::vector<std::tuple<int, std::vector<int>>>> build_queen_moves_by_layer(int width, int height)
{
  std::vector<std::vector<std::tuple<int, std::vector<int>>>> output(size_t(width) * size_t(height));

  for(const auto& queen_move : GameUtil::get_queen_moves(0, width, height))
    {
      int from_layer = std::get<0>(queen_move);
      int to_layer = std::get<1>(queen_move);
      const std::vector<int>& between = std::get<2>(queen_move);

      output.at(size_t(from_layer)).emplace_back(to_layer, between);
    }

  return output;
}

AmazonsGame::AmazonsGame(int width_in, int height_in)
  : ConfigNormalPlayGame(build_name(width_in, height_in)),
    width(width_in),
    height(height_in),
    queen_moves_by_layer(build_queen_moves_by_layer(width_in, height_in))
{
}

shared_dfa_ptr AmazonsGame::build_positions_reversed(shared_dfa_ptr positions_in) const
{
  // 0 empty and 3 arrow stay, 1 (side 0) and 2 (side 1) swap.
  std::vector<std::vector<int>> permutations(get_shape().size(), std::vector<int>({0, 2, 1, 3}));
  return DFAUtil::get_layer_permutation(positions_in, permutations);
}

std::string AmazonsGame::position_to_string(const DFAString& string_in) const
{
  std::ostringstream output;
  for(int y = height - 1; y >= 0; --y)
    {
      for(int x = 0; x < width; ++x)
	{
	  int square = x + width * y;
	  int layer = square + 0;
	  switch(string_in[layer])
	    {
	    case 0:
	      output << ".";
	      break;
	    case 1:
	      output << "w";
	      break;
	    case 2:
	      output << "b";
	      break;
	    case 3:
	      output << "*";
	      break;
	    }
	}
      output << "\n";
    }

  return output.str();
}

std::vector<DFAString> AmazonsGame::validate_moves(int side_to_move, const DFAString& position) const
{
  std::vector<DFAString> output;

  auto check_between = [&](const std::vector<int>& between_layers)
  {
    for(int layer : between_layers)
      {
        if(position[layer])
          {
            return false;
          }
      }

    return true;
  };

  size_t layers = get_shape().size();
  for(int from_layer = 0; from_layer < layers; ++from_layer)
    {
      if(position[from_layer] != 1 + side_to_move)
        {
          // from square not friendly piece
          continue;
        }

      for(const auto& queen_move : queen_moves_by_layer.at(size_t(from_layer)))
        {
          int to_layer = std::get<0>(queen_move);
          if(position[to_layer] != 0)
            {
              // to square not empty
              continue;
            }

          if(!check_between(std::get<1>(queen_move)))
            {
              // not a queen move
              continue;
            }

          for(const auto& queen_shot : queen_moves_by_layer.at(size_t(to_layer)))
            {
              int shot_layer = std::get<0>(queen_shot);
              if((shot_layer != from_layer) && (position[shot_layer] != 0))
                {
                  continue;
                }

              std::vector<int> shot_between;
              for(int shot_between_layer : std::get<1>(queen_shot))
                {
                  if(shot_between_layer != from_layer)
                    {
                      shot_between.push_back(shot_between_layer);
                    }
                }
              if(!check_between(shot_between))
                {
                  continue;
                }

              std::vector<int> move(layers);
              for(int i = 0; i < layers; ++i)
                {
                  move[i] = position[i];
                }
              move[from_layer] = 0;
              move[to_layer] = 1 + side_to_move;
              move[shot_layer] = 3;
              output.emplace_back(get_shape(), move);
            }
        }
    }

  return output;
}
