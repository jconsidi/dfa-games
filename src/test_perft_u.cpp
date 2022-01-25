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

void test_children(const Board& board, int depth_max);

void perft_u_board_helper(const Board& board, int depth, std::set<Board>& output)
{
  if(depth == 0)
    {
      output.insert(board);
      return;
    }

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);
  for(int i = 0; i < num_moves; ++i)
    {
      perft_u_board_helper(moves[i], depth - 1, output);
    }
}

uint64_t perft_u_board(const Board& board, int depth)
{
  std::set<Board> output;
  perft_u_board_helper(board, depth, output);
  return output.size();
}

void test(const Board& board, int depth_max)
{
  shared_dfa_ptr board_dfa = ChessGame::from_board(board);
  assert(board_dfa->size() == 1);

  ChessGame game;

  shared_dfa_ptr positions = board_dfa;
  for(int depth = 1; depth <= depth_max; ++depth)
    {
      std::cout << log_prefix << " depth " << depth << std::endl;

      uint64_t board_output = perft_u_board(board, depth);
      std::cout << log_prefix << "  board output = " << board_output << std::endl;

      int side_to_move = (board.get_side_to_move() + depth - 1) % 2;
      positions = game.get_moves_forward(side_to_move, positions);
      uint64_t dfa_output = positions->size();
      std::cout << log_prefix << "    dfa output = " << dfa_output << std::endl;

      if(dfa_output != board_output)
	{
	  std::cout << log_prefix << " board with depth " << depth << " discrepancy" << std::endl;
	  std::cout << board << std::endl;
	  std::cout << log_prefix << " side to move = " << board.get_side_to_move() << std::endl;

	  Board moves[CHESS_MAX_MOVES];
	  int num_moves = board.generate_moves(moves);

	  if(depth == 1)
	    {
	      // report a move that was not found

	      std::cout << log_prefix << " board dfa" << std::endl;
	      board_dfa->debug_example(std::cout);

	      shared_dfa_ptr expected_moves(new ChessGame::reject_dfa_type());

	      for(int i = 0; i < num_moves; ++i)
		{
		  shared_dfa_ptr move_dfa = ChessGame::from_board(moves[i]);
		  assert(move_dfa->size() == 1);
		  expected_moves = shared_dfa_ptr(new ChessGame::union_dfa_type(*expected_moves, *move_dfa));

		  if(ChessGame::intersection_dfa_type(*move_dfa, *positions).size() == 0)
		    {
		      std::cout << log_prefix << "found missing move:" << std::endl;
		      std::cout << moves[i];
		    }
		}

	      shared_dfa_ptr extra_moves(new ChessGame::difference_dfa_type(*positions, *expected_moves));
	      if(extra_moves->size())
		{
		  std::cout << log_prefix << "found " << extra_moves->size() << " extra moves" << std::endl;
		  extra_moves->debug_example(std::cout);
		}

	      shared_dfa_ptr missing_moves(new ChessGame::difference_dfa_type(*expected_moves, *positions));
	      if(missing_moves->size())
		{
		  std::cout << log_prefix << "found " << missing_moves->size() << " missing moves" << std::endl;
		  missing_moves->debug_example(std::cout);
		}

	      throw std::logic_error("found discrepancy");
	    }

	  for(int i = 0; i < num_moves; ++i)
	    {
	      std::cout << log_prefix << " testing child " << i << "/" << num_moves << std::endl;
	      assert(moves[i].get_side_to_move() != board.get_side_to_move());
	      test(moves[i], depth - 1);
	    }

	  throw std::logic_error("failed to find discrepancy");
	}
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
