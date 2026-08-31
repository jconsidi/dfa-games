// TicTacToeGame.cpp

#include "TicTacToeGame.h"

#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"

TicTacToeGame::TicTacToeGame(int n_in)
  : RectangularBase(n_in, n_in),
    RowMajorOrderBase(n_in, n_in),
    ConfigGame("tictactoe_" + std::to_string(n_in)),
    n(n_in)
{
}

std::string TicTacToeGame::position_to_string(const DFAString& position_in) const
{
  std::ostringstream builder;

  int index = 0;
  for(int row = 0; row < n; ++row)
    {
      for(int col = 0; col < n; ++col, ++index)
	{
	  int c = position_in[index];
	  if(c == 0)
	    {
	      builder << " ";
	    }
	  else
	    {
	      builder << (c - 1);
	    }

	  if(col + 1 < n)
	    {
	      builder << "|";
	    }
	}
      builder << std::endl;

      if(row + 1 < n)
	{
	  for(int i = 0; i < 2 * n - 1; ++i)
	    {
	      builder << ((i % 2 == 0) ? "-" : "+");
	    }
	  builder << std::endl;
	}
    }
  assert(index == n * n);

  return builder.str();
}

std::vector<DFAString> TicTacToeGame::validate_moves(int side_to_move, const DFAString& position) const
{
  std::vector<DFAString> output;
  if(validate_result(side_to_move, position))
    {
      return output;
    }

  int friendly_char = 1 + side_to_move;

  int n2 = n * n;
  for(int move = 0; move < n2; ++move)
    {
      if(position[move] != 0)
        {
          continue;
        }

      std::vector<int> position_new;
      for(int layer = 0; layer < n2; ++layer)
        {
          if(layer == move)
            {
              position_new.push_back(friendly_char);
            }
          else
            {
              position_new.push_back(position[layer]);
            }
        }

      output.emplace_back(get_shape(), position_new);
    }

  return output;
}

std::optional<int> TicTacToeGame::validate_result(int side_to_move, const DFAString& position) const
{
  int hostile_char = 1 + (1 - side_to_move);

  // check for other side having won
  auto check_helper = [&](int layer_first, int layer_delta)
  {
    if(position[layer_first] != hostile_char)
      {
        return std::optional<int>();
      }

    for(int i = 1; i < n; ++i)
      {
        if(position[layer_first + i * layer_delta] != hostile_char)
          {
            return std::optional<int>();
          }
      }

    return std::optional<int>(-1);
  };

  for(int row = 0; row < n; ++row)
    {
      auto row_result = check_helper(calculate_layer(row, 0), 1);
      if(row_result)
        {
          return row_result;
        }
    }

  for(int col = 0; col < n; ++col)
    {
      auto col_result = check_helper(calculate_layer(0, col), n);
      if(col_result)
        {
          return col_result;
        }
    }

  auto diag0_result = check_helper(0, n + 1);
  if(diag0_result)
    {
      return diag0_result;
    }

  auto diag1_result = check_helper(n - 1, n - 1);
  if(diag1_result)
    {
      return diag1_result;
    }

  int n2 = n * n;
  for(int layer = 0; layer < n2; ++layer)
    {
      if(position[layer] == 0)
        {
          // found empty square
          return std::optional<int>();
        }
    }

  return std::optional<int>(0);
}
