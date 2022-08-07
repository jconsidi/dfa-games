// test_chess_game.cpp

#include <iostream>

#include "ChessGame.h"
#include "Profile.h"
#include "test_utils.h"

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

#if CHESS_SQUARE_OFFSET == 2
  set_square(60); // white king position
  set_square(4); // black king position
#endif

  // black back line
  set_square(DFA_BLACK_ROOK_CASTLE);
  set_square(DFA_BLACK_KNIGHT);
  set_square(DFA_BLACK_BISHOP);
  set_square(DFA_BLACK_QUEEN);
  set_square(DFA_BLACK_KING);
  set_square(DFA_BLACK_BISHOP);
  set_square(DFA_BLACK_KNIGHT);
  set_square(DFA_BLACK_ROOK_CASTLE);
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
  set_square(DFA_WHITE_ROOK_CASTLE);
  set_square(DFA_WHITE_KNIGHT);
  set_square(DFA_WHITE_BISHOP);
  set_square(DFA_WHITE_QUEEN);
  set_square(DFA_WHITE_KING);
  set_square(DFA_WHITE_BISHOP);
  set_square(DFA_WHITE_KNIGHT);
  set_square(DFA_WHITE_ROOK_CASTLE);

  assert(next_square == 64 + CHESS_SQUARE_OFFSET);
  assert(output->size() == 1);

  return output;
}

int main()
{
  Profile profile("test_chess_game");
  ChessGame chess;

  // chess-specific tests

  profile.tic("get_initial_positions");
  shared_dfa_ptr initial_positions = chess.get_initial_positions();
  assert(initial_positions);
  assert(initial_positions->size() == 1);

  profile.tic("initial_positions_manual");
  shared_dfa_ptr initial_positions_expected = initial_positions_manual();
  assert(initial_positions_expected);
  assert(initial_positions_expected->size() == 1);

  profile.tic("initial_positions_check");
  shared_dfa_ptr initial_positions_check(new ChessGame::intersection_dfa_type(*initial_positions, *initial_positions_expected));
  assert(initial_positions_check);
  assert(initial_positions_check->size() == 1);

  profile.tic("black_check_board");
  Board black_check_board("rnbq1bnr/ppppkppp/8/4p3/8/BP6/P1PPPPPP/RN1QKBNR w KQkq - 0 1");
  assert(black_check_board.is_check(SIDE_BLACK));
  shared_dfa_ptr black_check_dfa = ChessGame::from_board(black_check_board);

  profile.tic("black_threat_12");
  shared_dfa_ptr black_threat_12 = chess.get_threat_positions(SIDE_BLACK, 12);

  profile.tic("black_threat_confirmed");
  shared_dfa_ptr black_threat_confirmed(new ChessGame::intersection_dfa_type(*black_check_dfa, *black_threat_12));
  assert(black_threat_confirmed->size() > 0);

  profile.tic("black_check_positions");
  shared_dfa_ptr black_check_positions = chess.get_check_positions(SIDE_BLACK);

  profile.tic("black_check_confirmed");
  shared_dfa_ptr black_check_confirmed(new ChessGame::intersection_dfa_type(*black_check_dfa, *black_check_positions));
  assert(black_check_confirmed->size() > 0);

  // generic tests

  profile.tic("positions_expected");
  std::vector<size_t> positions_expected({1, 20, 400});
  test_forward(chess, positions_expected);

  // done

  profile.tic("done");
  return 0;
}
