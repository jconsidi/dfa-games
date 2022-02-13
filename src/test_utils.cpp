// test_utils.cpp

#include "test_utils.h"

#include <iostream>

template<int ndim, int... shape_pack>
void test_game(const Game<ndim, shape_pack...>& game_in)
{
  std::string log_prefix = "test_game: ";
  std::cout << log_prefix << "ndim = " << ndim << std::endl;

  std::cout << log_prefix << "get_initial_positions()" << std::endl;
  auto initial_positions = game_in.get_initial_positions();
  assert(initial_positions);
  assert(initial_positions->size() == 1);

  std::cout << log_prefix << "get_moves_forward()" << std::endl;

  auto current_positions = initial_positions;
  for(int depth = 0; depth < 2; ++depth)
    {
      int side_to_move = depth % 2;
      current_positions = game_in.get_moves_forward(side_to_move, current_positions);
      std::cout << log_prefix << "depth " << (depth + 1) << ": " << current_positions->states() << " states, " << current_positions->size() << " positions" << std::endl;
    }
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template void test_game<CHESS_DFA_PARAMS>(const Game<CHESS_DFA_PARAMS>&);
template void test_game<TICTACTOE2_DFA_PARAMS>(const Game<TICTACTOE2_DFA_PARAMS>&);
template void test_game<TICTACTOE3_DFA_PARAMS>(const Game<TICTACTOE3_DFA_PARAMS>&);
template void test_game<TICTACTOE4_DFA_PARAMS>(const Game<TICTACTOE4_DFA_PARAMS>&);
