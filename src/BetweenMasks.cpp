// BetweenMasks.cpp

#include "BetweenMasks.h"

BetweenMasks::BetweenMasks()
{
  for(int i = 0; i < 64; ++i)
    {
      int i_rank = i / 8;
      int i_file = i % 8;

      for(int j = i + 1; j < 64; ++j)
	{
	  int j_rank = j / 8;
	  int j_file = j % 8;

	  int step = 0;

	  if(j_rank == i_rank)
	    {
	      // same rank = horizontal move
	      step = 1;
	    }
	  else if(j_file == i_file)
	    {
	      // same file = vertical move
	      step = 8;
	    }
	  else if(j_file - i_file == j_rank - i_rank)
	    {
	      // diagonal to bottom right
	      step = 9;
	    }
	  else if(i_file - j_file == j_rank - i_rank)
	    {
	      // diagonal to bottom left
	      step = 7;
	    }
	  else
	    {
	      // irregular move... so knight
	      continue;
	    }

	  BoardMask mask = 0ULL;
	  for(int k = i + step; k < j; k += step)
	    {
	      mask |= 1ULL << k;
	    }

	  masks[i][j] = mask;
	  masks[j][i] = mask;
	}
    }
}

const BetweenMasks between_masks;
