// test_chess_game.cpp

#include <iostream>
#include <string>

#include "ChessGame.h"
#include "DFAUtil.h"
#include "Profile.h"
#include "test_utils.h"

typedef typename ChessGame::shared_dfa_ptr shared_dfa_ptr;

shared_dfa_ptr initial_positions_manual()
{
  shared_dfa_ptr output = DFAUtil<CHESS_DFA_PARAMS>::get_accept();

  int next_square = 0;
  std::function<void(int)> set_square = [&](int character)
  {
    output = DFAUtil<CHESS_DFA_PARAMS>::get_intersection(output, DFAUtil<CHESS_DFA_PARAMS>::get_fixed(next_square, character));

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

  profile.tic("get_positions_initial");
  shared_dfa_ptr initial_positions = chess.get_positions_initial();
  assert(initial_positions);
  assert(initial_positions->size() == 1);

  for(auto iter = initial_positions->cbegin();
      iter < initial_positions->cend();
      ++iter)
    {
      auto position = *iter;
      std::string position_string = chess.position_to_string(position);
      std::cout << position_string << std::endl;

      assert(position_string ==
	     "r n b q k b n r\n"
	     "p p p p p p p p\n"
	     ". . . . . . . .\n"
	     ". . . . . . . .\n"
	     ". . . . . . . .\n"
	     ". . . . . . . .\n"
	     "P P P P P P P P\n"
	     "R N B Q K B N R\n");
    }

  profile.tic("initial_positions_manual");
  shared_dfa_ptr initial_positions_expected = initial_positions_manual();
  assert(initial_positions_expected);
  assert(initial_positions_expected->size() == 1);

  profile.tic("initial_positions_check");
  shared_dfa_ptr initial_positions_check = DFAUtil<CHESS_DFA_PARAMS>::get_intersection(initial_positions, initial_positions_expected);
  assert(initial_positions_check);
  assert(initial_positions_check->size() == 1);

  profile.tic("black_check_board");
  Board black_check_board("rnbq1bnr/ppppkppp/8/4p3/8/BP6/P1PPPPPP/RN1QKBNR w KQkq - 0 1");
  assert(black_check_board.is_check(SIDE_BLACK));
  shared_dfa_ptr black_check_dfa = ChessGame::from_board(black_check_board);

  profile.tic("black_threat_12");
  shared_dfa_ptr black_threat_12 = chess.get_positions_threat(SIDE_BLACK, 12);

  profile.tic("black_threat_confirmed");
  shared_dfa_ptr black_threat_confirmed = DFAUtil<CHESS_DFA_PARAMS>::get_intersection(black_check_dfa, black_threat_12);
  assert(black_threat_confirmed->size() > 0);

  profile.tic("black_check_positions");
  shared_dfa_ptr black_check_positions = chess.get_positions_check(SIDE_BLACK);

  profile.tic("black_check_confirmed");
  shared_dfa_ptr black_check_confirmed = DFAUtil<CHESS_DFA_PARAMS>::get_intersection(black_check_dfa, black_check_positions);
  assert(black_check_confirmed->size() > 0);

  // generic tests

  profile.tic("positions_expected");
  std::vector<size_t> positions_expected({1, 20, 400});
  test_forward(chess, positions_expected);

  // done

  profile.tic("done");
  return 0;
}
