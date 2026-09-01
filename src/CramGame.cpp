// CramGame.cpp

#include "CramGame.h"

#include <format>
#include <sstream>

static std::string build_name(int width, int height)
{
  return std::format("cram_{:d}x{:d}", width, height);
}

CramGame::CramGame(int width_in, int height_in)
  : RectangularBase(width_in, height_in),
    RowMajorOrderBase(width_in, height_in),
    ConfigNormalPlayGame(build_name(width_in, height_in))
{
}

std::string CramGame::position_to_string(const DFAString& string_in) const
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
	      output << "#";
	      break;
	    }
	}
      output << "\n";
    }

  return output.str();
}

std::vector<DFAString> CramGame::validate_moves(int, const DFAString& position) const
{
  const auto& shape = get_shape();
  auto ndim = shape.size();

  std::vector<DFAString> output;

  std::vector<int> position_staging(ndim);
  // copy original position
  for(int layer = 0; layer < ndim; ++layer)
    {
      position_staging[layer] = position[layer];
    }

  auto move_helper = [&](int layer1, int layer2)
  {
    assert(position_staging[layer1] == 0);
    assert(position_staging[layer2] == 0);

    // update staging
    position_staging[layer1] = 1;
    position_staging[layer2] = 1;

    // copy to output
    output.emplace_back(shape, position_staging);

    // restore staging to original position
    position_staging[layer2] = 0;
    position_staging[layer1] = 0;
  };

  // move generation

  for(int row1 = 0; row1 < height; ++row1)
    {
      for(int col1 = 0; col1 < width; ++col1)
	{
          int layer1 = calculate_layer(row1, col1);
          if(position[layer1])
            {
              // first position occupied
              continue;
            }

          if(row1 + 1 < height)
            {
              // check vertical
              int layer2 = layer1 + width;
              if(!position[layer2])
                {
                  move_helper(layer1, layer2);
                }
            }

          if(col1 + 1 < width)
            {
              // check horizontal
              int layer2 = layer1 + 1;
              if(!position[layer2])
                {
                  move_helper(layer1, layer2);
                }
            }
        }
    }
  
  return output;
}
