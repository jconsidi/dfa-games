// TicTacToeGame.cpp

#include "TicTacToeGame.h"

#include <sstream>
#include <string>
#include <vector>

#include "DFAUtil.h"

TicTacToeGame::TicTacToeGame(int n_in)
  : ConfigGame("tictactoe_" + std::to_string(n_in)),
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
