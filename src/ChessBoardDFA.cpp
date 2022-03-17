// ChessBoardDFA.cpp

#include "ChessBoardDFA.h"

#include <bit>

ChessBoardDFA::ChessBoardDFA(const Board& board_in)
{
  const auto& pieces_by_side = board_in.pieces_by_side;
  const auto& pieces_by_side_type = board_in.pieces_by_side_type;

#if CHESS_SQUARE_OFFSET == 2
  int white_king_position = std::countr_zero(pieces_by_side_type[SIDE_WHITE][PIECE_KING]);
  int black_king_position = std::countr_zero(pieces_by_side_type[SIDE_BLACK][PIECE_KING]);
#endif

  ChessDFACharacter board_characters[64];
  int next_square = 0;
  std::function<void(ChessDFACharacter)> set_square = [&](ChessDFACharacter character)
  {
    board_characters[next_square] = character;
    ++next_square;
  };

  for(int square = 0; square < 64; ++square)
    {
      int square_rank = square / 8;
      int square_file = square % 8;

      BoardMask square_mask = 1ULL << square;
      std::function<bool(int,int)> check = [=](int s, int p) {
	return (pieces_by_side_type[s][p] & square_mask) != 0;
      };
      if(pieces_by_side[SIDE_WHITE] & square_mask)
	{
	  if(check(SIDE_WHITE, PIECE_KING))
	    {
	      set_square(DFA_WHITE_KING);
	    }
	  else if(check(SIDE_WHITE, PIECE_QUEEN))
	    {
	      set_square(DFA_WHITE_QUEEN);
	    }
	  else if(check(SIDE_WHITE, PIECE_BISHOP))
	    {
	      set_square(DFA_WHITE_BISHOP);
	    }
	  else if(check(SIDE_WHITE, PIECE_KNIGHT))
	    {
	      set_square(DFA_WHITE_KNIGHT);
	    }
	  else if(check(SIDE_WHITE, PIECE_ROOK))
	    {
	      if(board_in.castling_availability & square_mask)
		{
		  set_square(DFA_WHITE_ROOK_CASTLE);
		}
	      else
		{
		  set_square(DFA_WHITE_ROOK);
		}
	    }
	  else if(check(SIDE_WHITE, PIECE_PAWN))
	    {
	      if((board_in.side_to_move == SIDE_BLACK) && (square_rank == 4) && (square_file == board_in.en_passant_file))
		{
		  set_square(DFA_WHITE_PAWN_EN_PASSANT);
		}
	      else
		{
		  set_square(DFA_WHITE_PAWN);
		}
	    }
	  else
	    {
	      // invalid board
	      assert(false);
	    }
	}
      else if(pieces_by_side[SIDE_BLACK] & square_mask)
	{
	  if(check(SIDE_BLACK, PIECE_KING))
	    {
	      set_square(DFA_BLACK_KING);
	    }
	  else if(check(SIDE_BLACK, PIECE_QUEEN))
	    {
	      set_square(DFA_BLACK_QUEEN);
	    }
	  else if(check(SIDE_BLACK, PIECE_BISHOP))
	    {
	      set_square(DFA_BLACK_BISHOP);
	    }
	  else if(check(SIDE_BLACK, PIECE_KNIGHT))
	    {
	      set_square(DFA_BLACK_KNIGHT);
	    }
	  else if(check(SIDE_BLACK, PIECE_ROOK))
	    {
	      if(board_in.castling_availability & square_mask)
		{
		  set_square(DFA_BLACK_ROOK_CASTLE);
		}
	      else
		{
		  set_square(DFA_BLACK_ROOK);
		}
	    }
	  else if(check(SIDE_BLACK, PIECE_PAWN))
	    {
	      if((board_in.side_to_move == SIDE_WHITE) && (square_rank == 3) && (square_file == board_in.en_passant_file))
		{
		  set_square(DFA_BLACK_PAWN_EN_PASSANT);
		}
	      else
		{
		  set_square(DFA_BLACK_PAWN);
		}
	    }
	  else
	    {
	      // invalid board
	      assert(false);
	    }
	}
      else
	{
	  set_square(DFA_BLANK);
	}
    }

  assert(next_square == 64);

  // last layer

  auto add_accept = [&](int layer, int character)
  {
    assert((0 <= character) && (character < this->get_layer_shape(layer)));

    if(layer == 63 + CHESS_SQUARE_OFFSET)
      {
	return this->add_state(layer, [=](int i)
	{
	  return i == character;
	});
      }
    else
      {
	return this->add_state(layer, [=](int i)
	{
	  return (i == character) ? 2 : 0;
	});
      }
  };

  for(int square = 63; square >= 0; --square)
    {
      int layer = square + CHESS_SQUARE_OFFSET;

      auto accept_index = add_accept(layer, board_characters[square]);
      if(layer > 0)
	{
	  assert(accept_index == 2);
	}
    }

#if CHESS_SQUARE_OFFSET == 2
  // black king index layer
  int black_king_accept_index = add_accept(1, black_king_position);
  assert(black_king_accept_index == 2);

  // white king index layer
  int white_king_accept_index = add_accept(0, white_king_position);
  assert(white_king_accept_index == 0);
#endif

  assert(this->size() == 1);
}
