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

void check_transition(shared_dfa_ptr before_expected, shared_dfa_ptr after_expected,
		      shared_dfa_ptr before_actual, shared_dfa_ptr after_actual)
{
  if(after_expected->size() == after_actual->size())
    {
      // assume we are good and skip expensive checks
      return;
    }

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

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);
  for(int i = 0; i < num_moves; ++i)
    {
      perft_u_board_helper(moves[i], depth + 1, depth_max, output);
    }
}

void test(const Board& board, int depth_max)
{
  std::vector<std::set<Board>> expected_boards(depth_max + 1);
  perft_u_board_helper(board, 0, depth_max, expected_boards);

  std::cout << log_prefix << " testing from_board()" << std::endl;

  std::vector<shared_dfa_ptr> expected_dfas;
  for(int depth = 0; depth <= depth_max; ++depth)
    {
      expected_dfas.push_back(boards_to_dfa(expected_boards[depth]));
      std::cout << log_prefix << "  depth " << depth << ": boards = " << expected_boards[depth].size() << ", expected dfa = " << expected_dfas[depth]->size() << std::endl;
      assert(expected_dfas[depth]->size() == expected_boards[depth].size());
    }

  std::cout << log_prefix << " testing get_moves_forward()" << std::endl;

  shared_dfa_ptr board_dfa = ChessGame::from_board(board);
  assert(board_dfa->size() == 1);

  ChessGame game;

  std::vector<shared_dfa_ptr> actual_dfas = {board_dfa};
  for(int depth = 1; depth <= depth_max; ++depth)
    {
      int side_to_move = (board.get_side_to_move() + depth - 1) % 2;
      actual_dfas.push_back(game.get_moves_forward(side_to_move, actual_dfas[depth - 1]));
      assert(actual_dfas.size() == depth + 1);
      std::cout << log_prefix << "  depth " << depth << ": expected = " << expected_dfas[depth]->size() << ", actual dfa = " << actual_dfas[depth]->size() << std::endl;

      check_transition(expected_dfas[depth - 1], expected_dfas[depth],
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
      // previous errors
      test("check detection", "rnbqkbnr/pppp1ppp/8/4p3/8/BP6/P1PPPPPP/RN1QKBNR b KQkq - 0 1", 1);
      test("TBD", "r1bqkbnr/pppppppp/n7/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1", 3);

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
