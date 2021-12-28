// MoveSet.cpp

#include "MoveSet.h"

MoveSet::MoveSet(bool (*check_func)(int, int, int, int))
{
  for(int from_index = 0; from_index < 64; ++from_index)
    {
      int from_rank = from_index / 8;
      int from_file = from_index % 8;

      BoardMask move_mask = 0ULL;

      for(int to_index = 0; to_index < 64; ++to_index)
	{
	  if(to_index == from_index)
	    {
	      continue;
	    }

	  int to_rank = to_index / 8;
	  int to_file = to_index % 8;

	  if((*check_func)(from_rank, from_file, to_rank, to_file))
	    {
	      move_mask |= 1ULL << to_index;
	    }
	}

      moves[from_index] = move_mask;
    }
}

MoveSet bishop_moves([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return abs(from_rank - to_rank) == abs(from_file - to_file);
 });

MoveSet king_moves([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return (abs(to_rank - from_rank) <= 1) && (abs(to_file - from_file) <= 1);
 });

MoveSet knight_moves([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return abs((to_rank - from_rank) * (to_file - from_file)) == 2;
 });

MoveSet pawn_advances_black([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return (to_file == from_file) && (to_rank == from_rank + 1);
 });
MoveSet pawn_advances_white([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return (to_file == from_file) && (to_rank == from_rank - 1);
 });

MoveSet pawn_captures_black([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return (to_rank == from_rank + 1) && (abs(to_file - from_file) == 1);
 });

MoveSet pawn_captures_white([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return (to_rank == from_rank - 1) && (abs(to_file - from_file) == 1);
 });

MoveSet queen_moves([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return (to_rank == from_rank) || (to_file == from_file) || abs(from_rank - to_rank) == abs(from_file - to_file);
 });

MoveSet rook_moves([](int from_rank, int from_file, int to_rank, int to_file)
 {
   return (to_rank == from_rank) || (to_file == from_file);
 });
