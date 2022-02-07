// test_perft_u.cpp

#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "ChessGame.h"
#include "PerftTestCases.h"
#include "chess.h"

typedef ChessGame::shared_dfa_ptr shared_dfa_ptr;

std::string log_prefix = "test_perft_u: ";

shared_dfa_ptr boards_to_dfa(const std::set<Board>& boards)
{
  std::cout << "converting " << boards.size() << " boards to dfas" << std::endl;
  std::vector<shared_dfa_ptr> board_dfas;
  std::for_each(boards.cbegin(), boards.cend(), [&](const Board& board)
  {
    board_dfas.push_back(ChessGame::from_board(board));
  });

  std::cout << "merging " << board_dfas.size() << " board dfas" << std::endl;
  return shared_dfa_ptr(new ChessGame::union_dfa_type(board_dfas));
}

void check_transition(int depth,
		      const std::set<Board>& before_boards, const std::set<Board>& after_boards,
		      shared_dfa_ptr before_actual, shared_dfa_ptr after_actual)
{
  std::cout << log_prefix << "  depth " << depth << ": boards = " << after_boards.size() << ", dfa = " << after_actual->size() << std::endl;

  if(after_boards.size() == after_actual->size())
    {
      // assume we are good and skip expensive checks
      return;
    }

  shared_dfa_ptr before_expected = boards_to_dfa(before_boards);
  assert(before_expected->size() == before_boards.size());
  assert(before_actual->size() == before_boards.size());

  // build expected DFA for comparison

  shared_dfa_ptr after_expected = boards_to_dfa(after_boards);

  // check for missing boards first since that is more common

  shared_dfa_ptr missing_boards(new ChessGame::difference_dfa_type(*after_expected, *after_actual));
  if(missing_boards->size())
    {
      std::cout << log_prefix << " found missing boards" << std::endl;
      missing_boards->debug_example(std::cout);
    }
  assert(missing_boards->size() == 0);

  // check for extra boards

  shared_dfa_ptr extra_boards(new ChessGame::difference_dfa_type(*after_actual, *after_expected));
  if(extra_boards->size())
    {
      std::cout << log_prefix << " found extra boards" << std::endl;
      extra_boards->debug_example(std::cout);
    }
  assert(extra_boards->size() == 0);

  // no differences found

  throw std::logic_error("failed to find transition difference");
}

void perft_u_board_helper(const Board& board, int depth, int depth_max, std::vector<std::set<Board>>& output)
{
  assert(depth <= depth_max);
  output[depth].insert(board);
  if(depth == depth_max)
    {
      return;
    }

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
  test(board, depth_max);
}

int main()
{
  try
    {
      // piece-specific tests
      test("rook castling white move", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq -", 4);
      test("rook castling black move", "r3k2r/8/8/8/8/8/8/R3K2R b KQkq -", 4);

      // previous errors
      test("check detection", "rnbqkbnr/pppp1ppp/8/4p3/8/BP6/P1PPPPPP/RN1QKBNR b KQkq - 0 1", 1);
      test("castling state", "r1bqkbnr/pppppppp/n7/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1", 3);

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
