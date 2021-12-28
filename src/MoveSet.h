// MoveSet.h

#ifndef MOVE_SET_H
#define MOVE_SET_H

#include "Board.h"

struct MoveSet
{
  BoardMask moves[64];

  MoveSet(bool (*)(int, int, int, int));
};

extern MoveSet bishop_moves;
extern MoveSet king_moves;
extern MoveSet knight_moves;
extern MoveSet pawn_advances_black;
extern MoveSet pawn_advances_white;
extern MoveSet pawn_captures_black;
extern MoveSet pawn_captures_white;
extern MoveSet queen_moves;
extern MoveSet rook_moves;

#endif
