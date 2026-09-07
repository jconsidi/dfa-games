// build_puzzle.cpp

#include <iostream>

#include "Puzzle.h"
#include "SlidingTilePuzzle.h"

int main()
{
  Puzzle *puzzle = new SlidingTilePuzzle(4, 4);

  shared_dfa_ptr won = puzzle->get_positions_won();
  std::cout << "WON: " << won->size() << " positions." << std::endl;

  int ply_max = 100;
  std::vector<shared_dfa_ptr> positions = {won};
  for(int ply = 1; ply <= ply_max; ++ply)
    {
      shared_dfa_ptr positions_new = puzzle->get_moves_backward(positions.at(ply - 1));
      positions.push_back(positions_new);

      auto size_new = positions_new->size();
      if(ply >= 2)
        {
          size_new -= positions.at(ply - 2)->size();
        }
      
      std::cout << "PLY: " << ply << ", POSITIONS: " << positions_new->size() << ", NEW: " << size_new << ", STATES: " << positions_new->states() << ", POSITIONS/STATE: " << (positions_new->size() / double(positions_new->states())) << std::endl;

      if((ply >= 2) && (positions_new->get_hash() == positions.at(ply - 2)->get_hash()))
        {
          break;
        }
    }

  return 0;
}
