// SlidingTilePuzzle.cpp

#include "SlidingTilePuzzle.h"

#include <format>
#include <sstream>

SlidingTilePuzzle::SlidingTilePuzzle(int width_in, int height_in)
  : RectangularBase(width_in, height_in),
    RowMajorOrderBase(width_in, height_in),
    ConfigPuzzle(std::format("slidingtile_{:d}x{:d}", width_in, height_in))
{
}

std::string SlidingTilePuzzle::position_to_string(const DFAString& string_in) const
{
  std::ostringstream output;
  for(int y = height - 1; y >= 0; --y)
    {
      for(int x = 0; x < width; ++x)
	{
	  int square = x + width * y;
	  int layer = square + 0;

          int c = string_in[layer];
          if(c)
            {
	      output << std::format("{:02d}", string_in[layer]);
            }
          else
            {
	      output << "  ";
            }
	}
      output << "\n";
    }

  return output.str();
}

std::vector<DFAString> SlidingTilePuzzle::validate_moves(int, const DFAString& position) const
{
  const auto& shape = get_shape();

  std::vector<DFAString> output;

  // move generation

  for(int row_from = 0; row_from < height; ++row_from)
    {
      for(int col_from = 0; col_from < width; ++col_from)
	{
	  int layer_from = calculate_layer(row_from, col_from);
          if(position[layer_from] != 0)
            {
              continue;
            }

          auto output_helper = [&](int row_to, int col_to)
          {
            int layer_to = calculate_layer(row_to, col_to);
            int moving_tile = position[layer_to];

            std::vector<int> position_new;
            for(int layer = 0; layer < shape.size(); ++layer)
              {
                if(layer == layer_from)
                  {
                    // moving tile goes here
                    position_new.push_back(moving_tile);
                  }
                else if(layer == layer_to)
                  {
                    // empty space goes here
                    position_new.push_back(0);
                  }
                else
                  {
                    position_new.push_back(position[layer]);
                  }
              }

            output.emplace_back(shape, position_new);
          };

          if(row_from > 0)
            {
              output_helper(row_from - 1, col_from);
            }
          if(row_from + 1 < height)
            {
              output_helper(row_from + 1, col_from);
            }
          if(col_from > 0)
            {
              output_helper(row_from, col_from - 1);
            }
          if(col_from + 1 < width)
            {
              output_helper(row_from, col_from + 1);
            }
        }
    }

  return output;
}
