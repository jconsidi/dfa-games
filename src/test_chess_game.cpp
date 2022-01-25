// test_chess_game.cpp

#include <iostream>

#include "ChessGame.h"

typedef typename ChessGame::shared_dfa_ptr shared_dfa_ptr;

shared_dfa_ptr initial_positions_manual()
{
  shared_dfa_ptr output(new ChessGame::accept_dfa_type());

  int next_square = 0;
  std::function<void(int)> set_square = [&](int character)
  {
    output = shared_dfa_ptr(new ChessGame::intersection_dfa_type(*output, ChessGame::fixed_dfa_type(next_square, character)));

    ++next_square;
  };

  set_square(60); // white king position
  set_square(4); // black king position
  // black back line
  set_square(DFA_BLACK_ROOK);
  set_square(DFA_BLACK_KNIGHT);
  set_square(DFA_BLACK_BISHOP);
  set_square(DFA_BLACK_QUEEN);
  set_square(DFA_BLACK_KING);
  set_square(DFA_BLACK_BISHOP);
  set_square(DFA_BLACK_KNIGHT);
  set_square(DFA_BLACK_ROOK);
  // black pawns
  for(int i = 0; i < 8; ++i)
    {
      set_square(DFA_BLACK_PAWN);
    }
  // empty center
  for(int i = 0; i < 32; ++i)
    {
      set_square(DFA_BLANK);
    }
  // white pawns
  for(int i = 0; i < 8; ++i)
    {
      set_square(DFA_WHITE_PAWN);
    }

  // white back line
  set_square(DFA_WHITE_ROOK);
  set_square(DFA_WHITE_KNIGHT);
  set_square(DFA_WHITE_BISHOP);
  set_square(DFA_WHITE_QUEEN);
  set_square(DFA_WHITE_KING);
  set_square(DFA_WHITE_BISHOP);
  set_square(DFA_WHITE_KNIGHT);
  set_square(DFA_WHITE_ROOK);

  assert(next_square == 66);
  assert(output->size() == 1);

  return output;
}

int main()
{
  ChessGame chess;

  shared_dfa_ptr initial_positions = chess.get_initial_positions();
  assert(initial_positions);
  assert(initial_positions->size() == 1);

  shared_dfa_ptr initial_positions_expected = initial_positions_manual();
  assert(initial_positions_expected);
  assert(initial_positions_expected->size() == 1);

  shared_dfa_ptr initial_positions_check(new ChessGame::intersection_dfa_type(*initial_positions, *initial_positions_expected));
  assert(initial_positions_check);
  assert(initial_positions_check->size() == 1);

  return 0;
}
