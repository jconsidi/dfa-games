// util.cpp

#include <bit>
#include <cassert>

#include "utils.h"

std::string mask_to_square(BoardMask mask)
{
  // map mask to a square name. assumes only one bit is set in the
  // mask.

  assert(std::popcount(mask) == 1);

  int index = std::countr_zero(mask);
  assert(index < 64);

  int rank = index / 8;
  int file = index % 8;

  std::string output;
  output += 'a' + file;
  output += '1' + (7 - rank);

  return output;
}

uint64_t perft(const Board& board, int depth)
{
  if(depth <= 0)
    return 1;

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);
  if(depth == 1)
    return num_moves;

  uint64_t output = 0;
  for(uint64_t i = 0; i < num_moves; ++i)
    {
      output += perft(moves[i], depth - 1);
    }
  return output;
}

std::string uci_move(const Board& before, const Board& after)
{
  // approximate UCI move string. will fail for castling and promotion.

  BoardMask flipped = before.pieces_by_side[before.side_to_move] ^ after.pieces_by_side[before.side_to_move];
  assert(flipped);
  BoardMask from = flipped & before.pieces_by_side[before.side_to_move];
  assert(from);
  BoardMask to = flipped & after.pieces_by_side[before.side_to_move];
  assert(to);

  if(std::popcount(from) > 1)
    {
      // must be castling
      assert(from & before.pieces_by_side_type[before.side_to_move][PIECE_KING]);
      from &= before.pieces_by_side_type[before.side_to_move][PIECE_KING];

      to &= after.pieces_by_side_type[before.side_to_move][PIECE_KING];
      assert(to);
    }

  assert(std::popcount(from) == 1);
  assert(std::popcount(to) == 1);

  std::string promotion;
  if(from & before.pieces_by_side_type[before.side_to_move][PIECE_PAWN])
    {
      // pawn moved
      if(!(to & after.pieces_by_side_type[before.side_to_move][PIECE_PAWN]))
	{
	  // no longer a pawn

	  if(to & after.pieces_by_side_type[before.side_to_move][PIECE_QUEEN])
	    {
	      promotion = "q";
	    }
	  else if(to & after.pieces_by_side_type[before.side_to_move][PIECE_ROOK])
	    {
	      promotion = "r";
	    }
	  else if(to & after.pieces_by_side_type[before.side_to_move][PIECE_KNIGHT])
	    {
	      promotion = "n";
	    }
	  else if(to & after.pieces_by_side_type[before.side_to_move][PIECE_BISHOP])
	    {
	      promotion = "b";
	    }
	}
    }

  return mask_to_square(from) + mask_to_square(to) + promotion;
}
