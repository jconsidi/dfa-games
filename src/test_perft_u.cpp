// test_perft_u.cpp

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "ChessGame.h"
#include "DFAUtil.h"
#include "PerftTestCases.h"
#include "chess.h"

typedef ChessGame::dfa_string_type dfa_string_type;
typedef ChessGame::shared_dfa_ptr shared_dfa_ptr;

Board get_example(int, shared_dfa_ptr);

std::string log_prefix = "test_perft_u: ";

shared_dfa_ptr boards_to_dfa(const std::set<Board>& boards)
{
  std::cout << "converting " << boards.size() << " boards to dfas" << std::endl;

  std::vector<dfa_string_type> board_strings;
  std::for_each(boards.cbegin(), boards.cend(), [&](const Board& board)
  {
    board_strings.push_back(ChessGame::from_board_to_dfa_string(board));
  });

  return DFAUtil<CHESS_DFA_PARAMS>::from_strings(board_strings);
}

void check_transition(int depth,
		      const std::set<Board>& before_boards, const std::set<Board>& after_boards,
		      shared_dfa_ptr before_actual, shared_dfa_ptr after_actual)
{
  std::cout << log_prefix << "  depth " << depth << ": boards = " << after_boards.size() << ", dfa = " << after_actual->size() << std::endl;

  if((after_boards.size() == after_actual->size()) && (after_boards.size() > 1024))
    {
      // assume we are good and skip expensive checks
      return;
    }

  shared_dfa_ptr before_expected = boards_to_dfa(before_boards);
  assert(before_expected->size() == before_boards.size());
  assert(before_actual->size() == before_boards.size());

  // build expected DFA for comparison

  shared_dfa_ptr after_expected = boards_to_dfa(after_boards);
  assert(after_expected->size() == after_boards.size());

  int side_to_move_after = depth % 2;

  // check for missing boards first since that is more common

  shared_dfa_ptr missing_boards = DFAUtil<CHESS_DFA_PARAMS>::get_difference(after_expected, after_actual);
  if(missing_boards->size())
    {
      std::cout << log_prefix << " found " << missing_boards->size() << " missing boards" << std::endl;

      Board missing_example = get_example(side_to_move_after, missing_boards);

      for(Board before_board : before_boards)
	{
	  bool found_match = false;

	  Board moves[CHESS_MAX_MOVES];
	  int num_moves = before_board.generate_moves(moves);
	  for(int i = 0; i < num_moves; ++i)
	    {
	      if(moves[i] == missing_example)
		{
		  found_match = true;
		  break;
		}
	    }

	  if(found_match)
	    {
	      std::cout << "BEFORE:\n" << before_board << std::endl;
	      break;
	    }
	}
      std::cout << "MISSING:\n" << missing_example << std::endl;
    }
  assert(missing_boards->size() == 0);

  // check for extra boards

  shared_dfa_ptr extra_boards = DFAUtil<CHESS_DFA_PARAMS>::get_difference(after_actual, after_expected);
  if(extra_boards->size())
    {
      std::cout << log_prefix << " found " << extra_boards->size() << " extra boards" << std::endl;
      if(before_actual->size() == 1)
	{
	  std::cout << "BEFORE:\n" << get_example(side_to_move_after, before_actual) << std::endl;
	}
      std::cout << "EXTRA:\n" << get_example(side_to_move_after, extra_boards) << std::endl;
    }
  assert(extra_boards->size() == 0);

  // no differences found
  assert(after_actual->size() == after_boards.size());
}

Board get_example(int side_to_move, shared_dfa_ptr positions_in)
{
  // convenience object

  ChessGame game;

  for(auto iter = positions_in->cbegin();
      iter < positions_in->cend();
      ++iter)
    {
      return game.position_to_board(side_to_move, *iter);
    }

  assert(0);
  return "n/a";
}

void test(const Board& board_in, int depth_max)
{
  std::vector<std::set<Board>> expected_boards(1);
  expected_boards[0].insert(board_in);

  shared_dfa_ptr board_dfa = ChessGame::from_board(board_in);
  assert(board_dfa->size() == 1);
  std::vector<shared_dfa_ptr> actual_dfas = {board_dfa};

  ChessGame game;

  for(int depth = 1; depth <= depth_max; ++depth)
    {
      expected_boards.emplace_back();
      assert(expected_boards.size() == depth + 1);

      // extend expected_boards
      std::for_each(expected_boards[depth - 1].cbegin(), expected_boards[depth - 1].cend(), [&](const Board& board_previous)
      {
	Board moves[CHESS_MAX_MOVES];
	int num_moves = board_previous.generate_moves(moves);
	for(int i = 0; i < num_moves; ++i)
	  {
	    expected_boards[depth].insert(moves[i]);
	  }
      });

      // extend actual_dfas
      int side_to_move = (board_in.get_side_to_move() + depth - 1) % 2;
      actual_dfas.push_back(game.get_moves_forward(side_to_move, actual_dfas[depth - 1]));
      assert(actual_dfas.size() == depth + 1);

      check_transition(depth,
		       expected_boards[depth - 1], expected_boards[depth],
		       actual_dfas[depth - 1], actual_dfas[depth]);
    }
}

void test(std::string name, std::string fen, int depth_max)
{
  std::cout << log_prefix << "checking " << name << std::endl;
  std::cout << log_prefix << " FEN = " << fen << std::endl;

  Board board(fen);
  std::cout << board << std::endl;
  test(board, depth_max);
}

int main(int argc, char **argv)
{
  try
    {
      if(argc > 1)
	{
	  std::string fen = argv[1];
	  int depth_max = (argc > 2) ? atoi(argv[2]) : 1;
	  test("manual", fen, depth_max);

	  return 0;
	}

      // piece-specific tests
      test("bishop moves", "2b1kb2/8/8/8/8/8/8/2B1KB2 w - - 0 1", 4);
      test("king moves", "4k3/8/8/8/8/8/8/4K3 w - - 0 1", 4);
      test("knight moves", "1n2k1n1/8/8/8/8/8/8/1N2K1N1 w - - 0 1", 4);
      test("pawn moves", "4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1", 6);
      test("queen moves", "3qk3/8/8/8/8/8/8/3QK3 w - - 0 1", 4);
      test("rook moves", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq -", 6);

      // previous errors
      test("check detection", "rnbqkbnr/pppp1ppp/8/4p3/8/BP6/P1PPPPPP/RN1QKBNR b KQkq - 0 1", 1);
      test("castling state", "r1bqkbnr/pppppppp/n7/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1", 3);
      test("capture promotion", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q2/PPPKBPpP/R1B4R b KQkq -", 1);
      test("en passant", "8/2p5/K5R1/1P3R2/7k/5R2/8/8 b - -", 2);
      test("castle rights", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 1);
      test("promotion", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q2/PPPKBPpP/R1B4R b kq - 0 1", 1);

      // perft cases validated in test_perft
      for(int i = 0; i < perft_test_cases.size(); ++i)
	{
	  const PerftTestCase& test_case = perft_test_cases.at(i);
	  const std::string& name = std::get<0>(test_case);
	  const std::string& fen = std::get<1>(test_case);
	  const std::vector<uint64_t>& expected_outputs = std::get<2>(test_case);

	  test(name, fen, expected_outputs.size());
	}
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }

  return 0;
}
