// GameUtil.cpp

#include "GameUtil.h"

#include <cmath>

int sign(int x)
{
  if(x > 0)
    {
      return 1;
    }

  if(x < 0)
    {
      return -1;
    }

  return 0;
}

std::vector<std::pair<int, int>> GameUtil::get_between(int x_begin, int y_begin, int x_end, int y_end)
{
  std::vector<std::pair<int, int>> output;

  int x_delta = sign(x_end - x_begin);
  int y_delta = sign(y_end - y_begin);

  if(x_delta && y_delta)
    {
      assert(abs(x_end - x_begin) == abs(y_end - y_begin));
    }

  for(int x = x_begin + x_delta, y = y_begin + y_delta;
      (x != x_end) || (y != y_end);
      x += x_delta, y += y_delta)
    {
      output.emplace_back(x, y);
    }

  return output;
}

const std::vector<std::tuple<int, int, std::vector<int>>>& GameUtil::get_queen_moves(int offset, int width, int height)
{
  static std::vector<std::tuple<int, int, std::vector<int>>> output;
  static int width_cache = 0;
  static int height_cache = 0;

  if(output.size())
    {
      assert(width == width_cache);
      assert(height == height_cache);
    }

  if(output.size() == 0)
    {
      for(int square_from = 0; square_from < width * height; ++square_from)
	{
	  int x_from = square_from % width;
	  int y_from = square_from / width;
	  int layer_from = square_from + offset;

	  for(int square_to = 0; square_to < width * height; ++square_to)
	    {
	      if(square_to == square_from)
		{
		  continue;
		}

	      int x_to = square_to % width;
	      int y_to = square_to / width;
	      int layer_to = square_to + offset;

	      if((x_from == x_to) || (y_from == y_to) || (abs(x_from - x_to) == abs(y_from - y_to)))
		{
		  int x_delta = sign(x_to - x_from);
		  int y_delta = sign(y_to - y_from);

		  int distance = (x_from != x_to) ? abs(x_from - x_to) : abs(y_from - y_to);

		  std::vector<int> between;
		  for(int i = 1; i < distance; ++i)
		    {
		      int x_mid = x_from + i * x_delta;
		      int y_mid = y_from + i * y_delta;
		      int square_mid = x_mid + width * y_mid;
		      int layer_mid = offset + square_mid;
		      between.push_back(layer_mid);
		    }

		  output.emplace_back(layer_from, layer_to, between);
		}
	    }
	}

      width_cache = width;
      height_cache = height;
    }

  return output;
}
