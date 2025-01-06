// Board.cpp

#include "Board.h"

#include <bit>
#include <ctype.h>
#include <iostream>

#include "BetweenMasks.h"
#include "MoveSet.h"

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

  // side to move

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

  // castling availability

  for(; *cp && *cp != ' '; ++cp)
    {
      switch(*cp)
	{
	case '-':
	  break;

	case 'K':
	  castling_availability |= 1ULL << 63;
	  break;

	case 'Q':
	  castling_availability |= 1ULL << 56;
	  break;

	case 'k':
	  castling_availability |= 1ULL << 7;
	  break;

	case 'q':
	  castling_availability |= 1ULL << 0;
	  break;

	default:
	  throw std::logic_error("unrecognized character for castling availability");
	}
    }

  if(*cp != ' ')
    {
      throw std::invalid_argument("FEN string did not have space after castling availability");
    }
  cp += 1;
}

Board::Board(std::string fen_in)
  : Board(fen_in.c_str())
{
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

  return (between_masks.masks[i][j] & pieces) != 0;
}

bool Board::finish_move()
{
  // update pieces mask

  pieces = pieces_by_side[SIDE_WHITE] | pieces_by_side[SIDE_BLACK];

  castling_availability &= pieces_by_side_type[SIDE_WHITE][PIECE_ROOK] | pieces_by_side_type[SIDE_BLACK][PIECE_ROOK];

  // make sure move was not into check or staying in check

  Side side_just_moved = side_flip(side_to_move);
  return !(is_check(side_just_moved));
}

void Board::move_piece(int from_index, int to_index)
{
  const BoardMask from_mask = 1ULL << from_index;
  const BoardMask to_mask = 1ULL << to_index;

  Side side_moving = side_flip(side_to_move);
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

	  if(i == PIECE_PAWN)
	    {
	      halfmove_clock = 0;
	    }

	  break;
	}
    }

  if(move_mask & pieces_by_side_type[side_moving][PIECE_KING])
    {
      // king moved so cannot castle anymore
      if(side_moving == SIDE_WHITE)
	{
	  castling_availability &= 0xFFULL << 0;
	}
      else
	{
	  castling_availability &= 0xFFULL << 56;
	}
    }

  // castling cleanup

  castling_availability &= ~(from_mask | to_mask);

  // clear captured pieces

  if(pieces_by_side[side_to_move] & to_mask)
    {
      const BoardMask to_mask_inv = ~to_mask;
      pieces_by_side[side_to_move] &= to_mask_inv;
      for(int i = 0; i < PIECE_MAX; ++i)
	{
	  if(pieces_by_side_type[side_to_move][i] & to_mask)
	    {
	      // found captured piece
	      pieces_by_side_type[side_to_move][i] &= to_mask_inv;
	      break;
	    }
	}

      halfmove_clock = 0;
    }
}

void Board::start_move(Board &move_out) const
{
  move_out = *this;

  move_out.side_to_move = side_flip(side_to_move);

  // will be overriden in generate_moves after pawn double advance
  move_out.en_passant_file = -1;

  // will be cleared in generate_moves after pawn moves or captures
  move_out.halfmove_clock += 1;

  if(side_to_move == SIDE_BLACK)
    {
      move_out.fullmove_number += 1;
    }
}

bool Board::try_castle(int king_index, int rook_index, Board &move_out) const
{
  if(check_between(king_index, rook_index))
    {
      // piece between king and rook
      return false;
    }

  if(is_attacked(side_to_move, king_index))
    {
      // cannot castle out of check
      return false;
    }

  int direction = (rook_index > king_index) ? 1 : -1;

  if(is_attacked(side_to_move, king_index + direction))
    {
      // cannot castle through check
      return false;
    }

  if(is_attacked(side_to_move, king_index + direction * 2))
    {
      // cannot castle into check
      return false;
    }

    start_move(move_out);
    move_out.move_piece(king_index, king_index + direction * 2);
    move_out.move_piece(rook_index, king_index + direction);

    return move_out.finish_move();
}

bool Board::try_move(int from, int to, Board &move_out) const
{
  if(check_between(from, to))
    {
      // piece between blocking this move.
      return false;
    }

  start_move(move_out);
  move_out.move_piece(from, to);

  // wrap up and check if valid

  return move_out.finish_move();
}

////////////////////////////////////////////////////////////
// public functions ////////////////////////////////////////
////////////////////////////////////////////////////////////

bool Board::operator<(const Board& other) const
{
  for(int side = 0; side < 2; ++side)
    {
      for(int piece = 0; piece < PIECE_MAX; ++piece)
	{
	  if(pieces_by_side_type[side][piece] < other.pieces_by_side_type[side][piece])
	    {
	      return true;
	    }
	  else if(pieces_by_side_type[side][piece] > other.pieces_by_side_type[side][piece])
	    {
	      return false;
	    }
	}
    }

  if(side_to_move < other.side_to_move)
    {
      return true;
    }
  else if(side_to_move > other.side_to_move)
    {
      return false;
    }

  if(en_passant_file < other.en_passant_file)
    {
      return true;
    }
  else if(en_passant_file > other.en_passant_file)
    {
      return false;
    }

  if(castling_availability < other.castling_availability)
    {
      return true;
    }
  else if(castling_availability > other.castling_availability)
    {
      return false;
    }

  return false;
}

bool Board::operator==(const Board& other) const
{
  for(int side = 0; side < 2; ++side)
    {
      for(int piece = 0; piece < PIECE_MAX; ++piece)
	{
	  if(pieces_by_side_type[side][piece] != other.pieces_by_side_type[side][piece])
	    {
	      return false;
	    }
	}
    }

  if(side_to_move != other.side_to_move)
    {
      return false;
    }

  if(en_passant_file != other.en_passant_file)
    {
      return false;
    }

  if(castling_availability != other.castling_availability)
    {
      return false;
    }

  return true;
}

int Board::generate_moves(Board moves_out[CHESS_MAX_MOVES]) const
{
  Side side_not_to_move = side_flip(side_to_move);

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
		     try_move(from_index, two_index, moves_out[move_count]))
		    {
		      moves_out[move_count].en_passant_file = from_index % 8;
		      ++move_count;
		    }
		}
	    }
	  else if(from_rank_by_side == 4)
	    {
	      // at the right rank for en-passant
	      if((en_passant_file >= 0) && (abs(from_file - en_passant_file) == 1))
		{
		  // can capture en-passant.

		  int capture_rank = (side_to_move == SIDE_WHITE) ? 3 : 4;
		  int capture_index = capture_rank * 8 + en_passant_file;
		  BoardMask capture_mask_inv = ~(1ULL << capture_index);

		  int to_rank = (side_to_move == SIDE_WHITE) ? 2 : 5;
		  int to_index = to_rank * 8 + en_passant_file;

		  Board& move = moves_out[move_count];
		  start_move(move);
		  move.move_piece(from_index, to_index);
		  move.pieces &= capture_mask_inv;
		  move.pieces_by_side[side_not_to_move] &= capture_mask_inv;
		  move.pieces_by_side_type[side_not_to_move][PIECE_PAWN] &= capture_mask_inv;

		  if(move.finish_move())
		    {
		      move_count += 1;
		    }
		}
	    }
	  else if(from_rank_by_side == 6)
	    {
	      // pawn promotion

	      Piece promotion_choices[4] = {PIECE_QUEEN, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK};

	      for(int to_index = std::countr_zero(to_mask);
		  to_index < 64;
		  to_index += 1 + std::countr_zero(to_mask >> (to_index + 1)))
		{
		  Board unpromoted;
		  start_move(unpromoted);
		  unpromoted.move_piece(from_index, to_index);

		  if(unpromoted.finish_move())
		    {
		      // move is valid, just need to pick what piece to promote now.

		      BoardMask to_mask = 1ULL << to_index;
		      BoardMask to_mask_inv = ~to_mask;

		      for(int i = 0; i < 4; ++i)
			{
			  Piece promotion_piece = promotion_choices[i];

			  Board &move = moves_out[move_count++] = unpromoted;
			  move.pieces_by_side_type[side_to_move][PIECE_PAWN] &= to_mask_inv;
			  move.pieces_by_side_type[side_to_move][promotion_piece] |= to_mask;
			}
		    }
		}

	      // already covered these moves
	      to_mask = 0;
	    }
	}
      else if(from_mask & pieces_by_side_type[side_to_move][PIECE_BISHOP])
	{
	  to_mask = bishop_moves.moves[from_index];
	}
      else if(from_mask & pieces_by_side_type[side_to_move][PIECE_KING])
	{
	  to_mask = king_moves.moves[from_index];

	  BoardMask rook_availability = castling_availability & pieces_by_side_type[side_to_move][PIECE_ROOK];
	  if(rook_availability)
	    {
	      if(side_to_move == SIDE_WHITE)
		{
		  if(rook_availability & (1ULL << 63))
		    {
		      // white king-side castling
		      if(try_castle(60, 63, moves_out[move_count]))
			{
			  move_count += 1;
			}
		    }

		  if(rook_availability & (1ULL << 56))
		    {
		      // white queen-side castling
		      if(try_castle(60, 56, moves_out[move_count]))
			{
			  move_count += 1;
			}
		    }
		}
	      else
		{
		  if(rook_availability & (1ULL << 7))
		    {
		      // black king-side castling
		      if(try_castle(4, 7, moves_out[move_count]))
			{
			  move_count += 1;
			}
		    }

		  if(rook_availability & (1ULL << 0))
		    {
		      // black queen-side castling
		      if(try_castle(4, 0, moves_out[move_count]))
			{
			  move_count += 1;
			}
		    }
		}
	    }
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
	  if(try_move(from_index, to_index, moves_out[move_count]))
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

int Board::get_side_to_move() const
{
  return side_to_move;
}

bool Board::is_attacked(Side defending_side, int defending_index) const
{
  Side attacking_side = side_flip(defending_side);

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

int Board::count_moves() const
{
  Board moves[CHESS_MAX_MOVES];
  return generate_moves(moves);
}

bool Board::is_check() const
{
  return is_check(side_to_move);
}

bool Board::is_check(Side defending_side) const
{
  int king_index = std::countr_zero(pieces_by_side_type[defending_side][PIECE_KING]);
  if(king_index >= 64)
    {
      // no king - allow for testing simplified positions
      return false;
    }

  return is_attacked(defending_side, king_index);
}

bool Board::is_checkmate() const
{
  return is_check() && (count_moves() == 0);
}

bool Board::is_draw() const
{
  return is_draw_by_rule() || is_draw_by_material();
}

bool Board::is_draw_by_material() const
{
  int piece_counts[2];
  for(int s = 0; s < 2; ++s)
    piece_counts[s] = std::popcount(pieces_by_side[s]);

  if((piece_counts[0] == 1) && (piece_counts[1] == 1))
    {
      // king vs king
      return true;
    }

  for(int s = 0; s < 2; ++s)
    {
      if((piece_counts[s] == 1) &&
	 (piece_counts[1-s] == 2) &&
	 pieces_by_side_type[1-s][PIECE_BISHOP])
	{
	  // king vs king and bishop
	  return true;
	}

      if((piece_counts[s] == 1) &&
	 (piece_counts[1-s] == 2) &&
	 pieces_by_side_type[1-s][PIECE_KNIGHT])
	{
	  // king vs king and knight
	  return true;
	}
    }

  return false;
}

bool Board::is_draw_by_rule() const
{
  return halfmove_clock >= 100;
}

bool Board::is_final() const
{
  return (count_moves() == 0) || is_draw();
}

bool Board::is_stalemate() const
{
  return !is_check() && (count_moves() == 0);
}

std::string Board::to_string() const
{
  std::string output("");

  for(int i = 0; i < 64; ++i)
    {
      BoardMask mask = 1ULL << i;
      char c = '.';
      for(int p = 0; p < 2; ++p)
	{
	  if(!(this->pieces_by_side[p] & mask))
	    continue;

	  if(this->pieces_by_side_type[p][PIECE_PAWN] & mask)
	    {
	      c = 'p';
	    }
	  else if(this->pieces_by_side_type[p][PIECE_KING] & mask)
	    {
	      c = 'k';
	    }
	  else if(this->pieces_by_side_type[p][PIECE_QUEEN] & mask)
	    {
	      c = 'q';
	    }
	  else if(this->pieces_by_side_type[p][PIECE_BISHOP] & mask)
	    {
	      c = 'b';
	    }
	  else if(this->pieces_by_side_type[p][PIECE_KNIGHT] & mask)
	    {
	      c = 'n';
	    }
	  else if(this->pieces_by_side_type[p][PIECE_ROOK] & mask)
	    {
	      c = 'r';
	    }
	}

      if(this->pieces_by_side[SIDE_WHITE] & mask)
	{
	  c = char(toupper(c));
	}

      // output piece or blank square
      output += c;

      // space before next square or new line
      if(i % 8 < 7)
	{
	  output += ' ';
	}
      else
	{
	  output += "\n";
	}
    }

  return output;
}

std::ostream& operator<<(std::ostream& os, const Board& board)
{
  os << board.to_string();
  return os;
}
