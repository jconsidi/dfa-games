// test_chess_game.cpp

#include <iostream>

#include "ChessGame.h"

typedef typename ChessGame::shared_dfa_ptr shared_dfa_ptr;

int main()
{
  ChessGame chess;

  shared_dfa_ptr initial_positions = chess.get_initial_positions();
  assert(initial_positions);
  assert(initial_positions->size() == 1);

  std::cout << " perft_u" << std::endl;

  std::vector<uint64_t> expected_sizes({20, 400});

  shared_dfa_ptr current_positions = initial_positions;
  for(int previous_ply = 0; previous_ply < expected_sizes.size(); ++previous_ply)
    {
      int side_to_move = previous_ply % 2;

      current_positions = chess.get_moves_forward(side_to_move, current_positions);
      int current_size = current_positions->size();
      int current_states = current_positions->states();

      std::cout << "  ply " << (previous_ply + 1) << ": " << current_size << " positions, " << current_states << " states" << std::endl;

      int expected_size = expected_sizes[previous_ply];
      if(current_size != expected_size)
	{
	  std::cerr << "  ply " << (previous_ply + 1) << ": " << expected_size << " expected" << std::endl;
	  return 1;
	}
    }

  return 0;
}
