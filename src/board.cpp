// board.cpp

#include <ctype.h>

#include <bit>
#include <iostream>

#include "chess.h"

////////////////////////////////////////////////////////////
// internal static data ////////////////////////////////////
////////////////////////////////////////////////////////////

struct MoveSet
{
  BoardMask moves[64];

  MoveSet(bool (*check_func)(int, int, int, int))
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
};

static MoveSet bishop_moves([](int from_rank, int from_file, int to_rank, int to_file)
			    {
			      return abs(from_rank - to_rank) == abs(from_file - to_file);
			    });

static MoveSet king_moves([](int from_rank, int from_file, int to_rank, int to_file)
			  {
			    return (abs(to_rank - from_rank) <= 1) && (abs(to_file - from_file) <= 1);
			  });

static MoveSet knight_moves([](int from_rank, int from_file, int to_rank, int to_file)
			    {
			      return abs((to_rank - from_rank) * (to_file - from_file)) == 2;
			    });

static MoveSet pawn_advances_black([](int from_rank, int from_file, int to_rank, int to_file)
				   {
				     return (to_file == from_file) && (to_rank == from_rank + 1);
				   });
static MoveSet pawn_advances_white([](int from_rank, int from_file, int to_rank, int to_file)
				   {
				     return (to_file == from_file) && (to_rank == from_rank - 1);
				   });

static MoveSet pawn_captures_black([](int from_rank, int from_file, int to_rank, int to_file)
				   {
				     return (to_rank == from_rank + 1) && (abs(to_file - from_file) == 1);
				   });

static MoveSet pawn_captures_white([](int from_rank, int from_file, int to_rank, int to_file)
				   {
				     return (to_rank == from_rank - 1) && (abs(to_file - from_file) == 1);
				   });

static MoveSet queen_moves([](int from_rank, int from_file, int to_rank, int to_file)
			   {
			     return (to_rank == from_rank) || (to_file == from_file) || abs(from_rank - to_rank) == abs(from_file - to_file);
			   });

static MoveSet rook_moves([](int from_rank, int from_file, int to_rank, int to_file)
			  {
			    return (to_rank == from_rank) || (to_file == from_file);
			  });

////////////////////////////////////////////////////////////
// constructors ////////////////////////////////////////////
////////////////////////////////////////////////////////////

Board::Board(const char *fen_string)
{
  const char *cp = fen_string;
  int bit_index = 0;
  for(; bit_index < 64 && *cp; ++cp)
    {
      const char c = *cp;

      // check for non-piece / spatial information.
      switch(c)
	{
	case ' ':
	  throw std::invalid_argument("found space before end of board squares");
	  
	case '/':
	  // spot check that we are at the end of a row.
	  if(bit_index % 8 != 0)
	    {
	      throw std::invalid_argument("found slash but not at end of a rank");
	    }
	  continue;

	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	  // skip ahead the specified number of spaces.
	  bit_index += c - '0';
	  continue;
	}

      Side p = isupper(c) ? SIDE_WHITE : SIDE_BLACK;
      BoardMask m = 1ULL << bit_index;

      pieces |= m;
      pieces_by_side[p] |= m;
      switch(tolower(c))
	{
	case 'b':
	  pieces_by_side_type[p][PIECE_BISHOP] |= m;
	  break;
	case 'k':
	  pieces_by_side_type[p][PIECE_KING] |= m;
	  break;
	case 'n':
	  pieces_by_side_type[p][PIECE_KNIGHT] |= m;
	  break;
	case 'p':
	  pieces_by_side_type[p][PIECE_PAWN] |= m;
	  break;
	case 'r':
	  pieces_by_side_type[p][PIECE_ROOK] |= m;
	  break;
	case 'q':
	  pieces_by_side_type[p][PIECE_QUEEN] |= m;
	  break;
	default:
	  throw std::invalid_argument("unrecognized piece character");
	}

      ++bit_index;
    }

  if(bit_index < 64)
    {
      throw std::invalid_argument("FEN string ended before board squares filled");
    }

  if(*cp != ' ')
    {
      throw std::invalid_argument("FEN string did not have space after board squares");
    }
  cp += 1;

  if(*cp == 'w')
    {
      side_to_move = SIDE_WHITE;
    }
  else if(*cp == 'b')
    {
      side_to_move = SIDE_BLACK;
    }
  else
    {
      throw std::invalid_argument("FEN string did not have expected side to move");
    }
  cp += 1;

  if(*cp != ' ')
    {
      throw std::invalid_argument("FEN string did not have space after side to move");
    }
  cp += 1;
}

////////////////////////////////////////////////////////////
// private functions ///////////////////////////////////////
////////////////////////////////////////////////////////////

bool Board::check_between(int i, int j) const
{
  if(i == j)
    {
      throw std::logic_error("check_between called with one square twice");
    }

  // swap if needed for i <= j so directions are constrained.
  if(i > j)
    {
      int temp = i;
      i = j;
      j = temp;
    }

  int i_rank = i / 8;
  int i_file = i % 8;

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
      return false;
    }
  for(int k = i + step; k < j; k += step)
    {
      if(pieces & (1ULL << k))
	{
	  return true;
	}
    }
  
  return false;
}

bool Board::finish_move()
{
  // update pieces mask

  pieces = pieces_by_side[SIDE_WHITE] | pieces_by_side[SIDE_BLACK];
  
  // make sure move was not into check or staying in check

  Side side_just_moved = (side_to_move == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;
  return !(is_in_check(side_just_moved));
}

void Board::move_piece(int from_index, int to_index)
{
  const BoardMask from_mask = 1ULL << from_index;
  const BoardMask to_mask = 1ULL << to_index;

  Side side_moving = (side_to_move == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;
  if(!(pieces_by_side[side_moving] & from_mask))
    {
      throw std::logic_error("tried to move without side piece");
    }
  if(pieces_by_side[side_moving] & to_mask)
    {
      throw std::logic_error("tried to move on top of same side piece");
    }

  // move_mask is XOR'd against this piece's masks to turn off "from"
  // bit and turn on "to" bit.

  const BoardMask move_mask = from_mask ^ to_mask;
  
  pieces_by_side[side_moving] ^= move_mask;
  for(int i = 0; i < PIECE_MAX; ++i)
    {
      if(pieces_by_side_type[side_moving][i] & from_mask)
	{
	  pieces_by_side_type[side_moving][i] ^= move_mask;
	  break;
	}
    }

  // clear captured pieces

  if(pieces_by_side[side_to_move] & to_mask)
    {
      const BoardMask to_mask_inv = ~to_mask;
      pieces_by_side[side_to_move] &= to_mask_inv;
      for(int i = 0; i < PIECE_MAX; ++i)
	{
	  if(pieces_by_side_type[i][side_to_move] & to_mask)
	    {
	      // found captured piece
	      pieces_by_side_type[side_to_move][i] &= to_mask_inv;
	      break;
	    }
	}
    }
}

void Board::start_move(Board *move_out) const
{
  *move_out = *this;

  move_out->side_to_move = (side_to_move == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;

  // will be overriden in generate_moves after pawn double advance
  move_out->en_passant_file = -1;
}

bool Board::try_move(int from, int to, Board *move_out) const
{
  if(check_between(from, to))
    {
      // piece between blocking this move.
      return false;
    }
  
  start_move(move_out);
  move_out->move_piece(from, to);

  // wrap up and check if valid

  return move_out->finish_move();
}

////////////////////////////////////////////////////////////
// public functions ////////////////////////////////////////
////////////////////////////////////////////////////////////

int Board::generate_moves(Board moves_out[CHESS_MAX_MOVES]) const
{
  Side side_not_to_move = (side_to_move == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;

  int move_count = 0;
  for(int from_index = 0; from_index < 64; ++from_index)
    {
      const BoardMask from_mask = 1ULL << from_index;
      if(!(from_mask & pieces_by_side[side_to_move]))
	{
	  continue;
	}

      BoardMask to_mask = 0ULL;
      if(from_mask & pieces_by_side_type[side_to_move][PIECE_PAWN])
	{
	  const MoveSet &pawn_advances = (side_to_move == SIDE_WHITE) ? pawn_advances_white : pawn_advances_black;
	  BoardMask advance_mask = pawn_advances.moves[from_index] & ~pieces;

	  const MoveSet &pawn_captures = (side_to_move == SIDE_WHITE) ? pawn_captures_white : pawn_captures_black;
	  BoardMask capture_mask = pawn_captures.moves[from_index] & pieces_by_side[side_not_to_move];

	  to_mask = advance_mask | capture_mask;

	  // advancing two ranks and en-passant setup
	  int from_rank = from_index / 8;
	  int from_file = from_index % 8;

	  int from_rank_by_side = (side_to_move == SIDE_WHITE) ? (7 - from_rank) : from_rank;
	  if(from_rank_by_side == 1)
	    {
	      // starting rank
	      if(advance_mask)
		{
		  // can advance one rank. try advancing two ranks too.

		  int two_index = from_index + ((side_to_move == SIDE_WHITE) ? -16 : +16);
		  BoardMask two_mask = 1ULL << two_index;

		  if(!(two_mask & pieces) &&
		     try_move(from_index, two_index, &moves_out[move_count]))
		    {
		      moves_out[move_count].en_passant_file = from_index % 8;
		      ++move_count;
		    }
		}
	    }
	}
      else if(from_mask & pieces_by_side_type[side_to_move][PIECE_BISHOP])
	{
	  to_mask = bishop_moves.moves[from_index];
	}
      else if(from_mask & pieces_by_side_type[side_to_move][PIECE_KING])
	{
	  to_mask = king_moves.moves[from_index];
	}
      else if(from_mask & pieces_by_side_type[side_to_move][PIECE_KNIGHT])
	{
	  to_mask = knight_moves.moves[from_index];
	}
      else if(from_mask & pieces_by_side_type[side_to_move][PIECE_QUEEN])
	{
	  to_mask = queen_moves.moves[from_index];
	}
      else if(from_mask & pieces_by_side_type[side_to_move][PIECE_ROOK])
	{
	  to_mask = rook_moves.moves[from_index];
	}
      else
	{
	  throw std::logic_error("unrecognized from piece type in generate_moves");
	}

      // filter out self-captures
      to_mask &= ~pieces_by_side[side_to_move];

      // try the remaining moves
      for(int to_index = std::countr_zero(to_mask);
	  to_index < 64;
	  to_index += 1 + std::countr_zero(to_mask >> (to_index + 1)))
	{
	  if(try_move(from_index, to_index, &moves_out[move_count]))
	    {
	      move_count += 1;
	    }
	}
    }

  if(move_count >= CHESS_MAX_MOVES)
    {
      throw std::logic_error("found more moves than CHESS_MAX_MOVES");
    }

  return move_count;
}

bool Board::is_attacked(Side defending_side, int defending_index) const
{
  Side attacking_side = defending_side == SIDE_WHITE ? SIDE_BLACK : SIDE_WHITE;

  // identify pieces that could attack if not blocked
  BoardMask attacking_mask = 0ULL;
  attacking_mask |= bishop_moves.moves[defending_index] & pieces_by_side_type[attacking_side][PIECE_BISHOP];
  attacking_mask |= king_moves.moves[defending_index] & pieces_by_side_type[attacking_side][PIECE_KING];
  attacking_mask |= knight_moves.moves[defending_index] & pieces_by_side_type[attacking_side][PIECE_KNIGHT];
  attacking_mask |= queen_moves.moves[defending_index] & pieces_by_side_type[attacking_side][PIECE_QUEEN];
  attacking_mask |= rook_moves.moves[defending_index] & pieces_by_side_type[attacking_side][PIECE_ROOK];

  // pawn captures. deliberately swapping color to reverse attack.
  MoveSet &pawn_captures = (attacking_side == SIDE_WHITE) ? pawn_captures_black : pawn_captures_white;
  attacking_mask |= pawn_captures.moves[defending_index] & pieces_by_side_type[attacking_side][PIECE_PAWN];

  // check possible attackers for blocks
  for(int attacking_index = std::countr_zero(attacking_mask);
      attacking_index < 64;
      attacking_index += 1 + std::countr_zero(attacking_mask >> (attacking_index + 1)))
    {
      if(!check_between(attacking_index, defending_index))
	{
	  // no blocking pieces
	  return true;
	}
    }

  // no unblocked attacks
  return false;
}

bool Board::is_in_check(Side defending_side) const
{
  int king_index = std::countr_zero(pieces_by_side_type[defending_side][PIECE_KING]);
  if(king_index >= 64)
    {
      throw std::logic_error("board missing king");
    }
  
  return is_attacked(defending_side, king_index);
}

std::ostream& operator<<(std::ostream& os, const Board& board)
{
  for(int i = 0; i < 64; ++i)
    {
      BoardMask mask = 1ULL << i;
      char c = '.';
      for(int p = 0; p < 2; ++p)
	{
	  if(!(board.pieces_by_side[p] & mask))
	    continue;

	  if(board.pieces_by_side_type[p][PIECE_PAWN] & mask)
	    {
	      c = 'p';
	    }
	  else if(board.pieces_by_side_type[p][PIECE_KING] & mask)
	    {
	      c = 'k';
	    }
	  else if(board.pieces_by_side_type[p][PIECE_QUEEN] & mask)
	    {
	      c = 'q';
	    }
	  else if(board.pieces_by_side_type[p][PIECE_BISHOP] & mask)
	    {
	      c = 'b';
	    }
	  else if(board.pieces_by_side_type[p][PIECE_KNIGHT] & mask)
	    {
	      c = 'n';
	    }
	  else if(board.pieces_by_side_type[p][PIECE_ROOK] & mask)
	    {
	      c = 'r';
	    }
	}

      if(board.pieces_by_side[SIDE_WHITE] & mask)
	{
	  c = toupper(c);
	}

      // output piece or blank square
      os << c;

      // space before next square or new line
      if(i % 8 < 7)
	{
	  os << ' ';
	}
      else
	{
	  os << std::endl;
	}
    }

  return os;
}
