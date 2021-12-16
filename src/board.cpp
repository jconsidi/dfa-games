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

static bool check_bishop_move(int from_rank, int from_file, int to_rank, int to_file)
{
  return abs(from_rank - to_rank) == abs(from_file - to_file);
}
static MoveSet bishop_moves(&check_bishop_move);

static bool check_king_move(int from_rank, int from_file, int to_rank, int to_file)
{
  return (abs(to_rank - from_rank) <= 1) && (abs(to_file - from_file) <= 1);
}
static MoveSet king_moves(&check_king_move);

static bool check_knight_move(int from_rank, int from_file, int to_rank, int to_file)
{
  return abs((to_rank - from_rank) * (to_file - from_file)) == 2;
}
static MoveSet knight_moves(&check_knight_move);

static bool check_queen_move(int from_rank, int from_file, int to_rank, int to_file)
{
  return (to_rank == from_rank) || (to_file == from_file) || abs(from_rank - to_rank) == abs(from_file - to_file);
}
static MoveSet queen_moves(&check_queen_move);

static bool check_rook_move(int from_rank, int from_file, int to_rank, int to_file)
{
  return (to_rank == from_rank) || (to_file == from_file);
}
static MoveSet rook_moves(&check_rook_move);

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

bool Board::try_move(int from, int to, Board *move_out) const
{
  if(check_between(from, to))
    {
      // piece between blocking this move.
      return false;
    }

  const BoardMask from_mask = 1ULL << from;
  const BoardMask to_mask = 1ULL << to;
  
  if(!(pieces_by_side[side_to_move] & from_mask))
    {
      throw std::logic_error("tried to move without side piece");
    }
  if(pieces_by_side[side_to_move] & to_mask)
    {
      throw std::logic_error("tried to move on top of same side piece");
    }

  // move_mask is XOR'd against this piece's masks to turn off "from"
  // bit and turn on "to" bit.

  const BoardMask move_mask = from_mask ^ to_mask;
  
  move_out->pieces_by_side[side_to_move] = pieces_by_side[side_to_move] ^ move_mask;
  for(int i = 0; i < PIECE_MAX; ++i)
    {
      if(pieces_by_side_type[side_to_move][i] & from_mask)
	{
	  // found the piece mask, so copy and flip bits
	  move_out->pieces_by_side_type[side_to_move][i] = pieces_by_side_type[side_to_move][i] ^ move_mask;
	}
      else
	{
	  // piece not here, so just copy
	  move_out->pieces_by_side_type[side_to_move][i] = pieces_by_side_type[side_to_move][i];
	}
    }

  // clear captured pieces

  const Side side_not_to_move = move_out->side_to_move = side_to_move == SIDE_WHITE ? SIDE_BLACK : SIDE_WHITE;

  const BoardMask to_mask_inv = ~to_mask;
  move_out->pieces_by_side[side_not_to_move] = pieces_by_side[side_not_to_move] & to_mask_inv;
  for(int i = 0; i < PIECE_MAX; ++i)
    {
      if(pieces_by_side_type[i][side_not_to_move] & to_mask)
	{
	  // found captured piece
	  move_out->pieces_by_side_type[side_not_to_move][i] = pieces_by_side_type[side_not_to_move][i] & to_mask_inv;
	}
      else
	{
	  // just copy
	  move_out->pieces_by_side_type[side_not_to_move][i] = pieces_by_side_type[side_not_to_move][i];
	}
    }

  // TODO special handling of castling and en-passant

  // almost done

  move_out->pieces = move_out->pieces_by_side[SIDE_WHITE] | move_out->pieces_by_side[SIDE_BLACK];
  
  // make sure move was not into check or staying in check

  return !(move_out->is_in_check(side_to_move));
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

      int from_rank = from_index / 8;
      int from_file = from_index % 8;

      BoardMask to_mask = 0ULL;
      if(from_mask & pieces_by_side_type[side_to_move][PIECE_BISHOP])
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
      if(to_mask)
	{
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

	  continue;
	}

      for(int to_index = 0; to_index < 64; ++to_index)
	{
	  const BoardMask to_mask = 1ULL << to_index;
	  if(to_mask & pieces_by_side[side_to_move])
	    {
	      // can't move onto pieces of own side
	      continue;
	    }

	  bool capture = (to_mask & pieces_by_side[side_not_to_move]) != 0;
	       
	  int to_rank = to_index / 8;
	  int to_file = to_index % 8;

	  if(from_mask & pieces_by_side_type[side_to_move][PIECE_PAWN])
	    {
	      if(side_to_move == SIDE_WHITE)
		{
		  if((from_rank == 6) && (to_rank == 4) && (to_file == from_file) && !capture)
		    {
		      // pushing two
		      if(try_move(from_index, to_index, &moves_out[move_count]))
			{
			  move_count += 1;
			}
		    }
		  else if(to_rank == from_rank - 1)
		    {
		      if((to_file == from_file) && !capture)
			{
			  // pushing pawn
			  if(try_move(from_index, to_index, &moves_out[move_count]))
			    {
			      move_count += 1;
			    }			  
			}
		      else if((abs(to_file - from_file) == 1) && capture)
			{
			  // pawn capture
			  if(try_move(from_index, to_index, &moves_out[move_count]))
			    {
			      move_count += 1;
			    }			  
			}
		    }
		}
	      else
		{
		  if((from_rank == 1) && (to_rank == 3) && (to_file == from_file) && !capture)
		    {
		      // pushing two
		      if(try_move(from_index, to_index, &moves_out[move_count]))
			{
			  move_count += 1;
			}
		    }
		  else if(to_rank == from_rank + 1)
		    {
		      if((to_file == from_file) && !capture)
			{
			  // pushing pawn
			  if(try_move(from_index, to_index, &moves_out[move_count]))
			    {
			      move_count += 1;
			    }			  
			}
		      else if((abs(to_file - from_file) == 1) && capture)
			{
			  // pawn capture
			  if(try_move(from_index, to_index, &moves_out[move_count]))
			    {
			      move_count += 1;
			    }			  
			}
		    }
		}		
	    }
	  else
	    {
	      throw std::logic_error("unrecognized from piece type in generate_moves");
	    }
	}
    }

  return move_count;
}

bool Board::is_in_check(Side side) const
{
  int king_index = std::countr_zero(pieces_by_side_type[side][PIECE_KING]);
  if(king_index >= 64)
    {
      throw std::logic_error("board missing king");
    }
  
  return false;
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
