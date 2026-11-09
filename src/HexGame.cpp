// HexGame.cpp

#include "HexGame.h"

#include <format>
#include <sstream>
#include <vector>

HexGame::HexGame(int n_in)
  : RectangularBase(n_in, n_in),
    RowMajorOrderBase(n_in, n_in),
    ConfigNormalPlayGame(std::format("hex_{:d}", n_in)),
    n(n_in)
{
}

// Whether side has joined its two edges: side 0 joins row 0 to row n - 1,
// side 1 joins column 0 to column n - 1. Flood fill from the starting edge.
bool HexGame::check_connected(int side, const DFAString& position) const
{
  int stone = side + 1;

  std::vector<bool> seen(n * n, false);
  std::vector<std::pair<int, int>> stack;

  for(int i = 0; i < n; ++i)
    {
      int row = (side == 0) ? 0 : i;
      int col = (side == 0) ? i : 0;
      int layer = calculate_layer(row, col);
      if(position[layer] == stone)
        {
          seen[layer] = true;
          stack.emplace_back(row, col);
        }
    }

  static const int offsets[6][2] = {{-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}};

  while(stack.size() > 0)
    {
      auto [row, col] = stack.back();
      stack.pop_back();

      if(((side == 0) ? row : col) == n - 1)
        {
          return true;
        }

      for(const auto& offset : offsets)
        {
          int row2 = row + offset[0];
          int col2 = col + offset[1];
          if((row2 < 0) || (row2 >= n) || (col2 < 0) || (col2 >= n))
            {
              continue;
            }

          int layer2 = calculate_layer(row2, col2);
          if((position[layer2] == stone) && !seen[layer2])
            {
              seen[layer2] = true;
              stack.emplace_back(row2, col2);
            }
        }
    }

  return false;
}

std::string HexGame::position_to_string(const DFAString& position_in) const
{
  // rhombus: each row shifted half a cell right of the one above, so the
  // neighbours of a cell are the two beside it and two each above and below.
  std::ostringstream output;
  for(int row = 0; row < n; ++row)
    {
      output << std::string(row, ' ');
      for(int col = 0; col < n; ++col)
        {
          if(col > 0)
            {
              output << " ";
            }

          int c = position_in[calculate_layer(row, col)];
          output << ((c == 0) ? '.' : char('0' + (c - 1)));
        }
      output << "\n";
    }

  return output.str();
}

std::vector<DFAString> HexGame::validate_moves(int side_to_move, const DFAString& position) const
{
  std::vector<DFAString> output;
  if(check_connected(1 - side_to_move, position))
    {
      return output;
    }

  int n2 = n * n;
  std::vector<int> position_staging(n2);
  for(int layer = 0; layer < n2; ++layer)
    {
      position_staging[layer] = position[layer];
    }

  for(int layer = 0; layer < n2; ++layer)
    {
      if(position_staging[layer] != 0)
        {
          continue;
        }

      position_staging[layer] = side_to_move + 1;
      output.emplace_back(get_shape(), position_staging);
      position_staging[layer] = 0;
    }

  return output;
}
