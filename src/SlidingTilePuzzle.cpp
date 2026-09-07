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
