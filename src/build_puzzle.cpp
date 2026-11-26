// build_puzzle.cpp

#include <format>
#include <iostream>

#include "DFAUtil.h"
#include "Puzzle.h"
#include "game_utils.h"

int main(int argc, char **argv)
{
  if(argc != 2)
    {
      std::cerr << "usage: build_puzzle PUZZLE_NAME\n";
      return 1;
    }

  Puzzle *puzzle = get_puzzle(argv[1]);

  shared_dfa_ptr won = puzzle->get_positions_won();
  std::cout << "WON: " << won->size() << " positions." << std::endl;

  // ply=NNN: the positions from which exactly NNN moves reach a won position.
  // ply_max=NNN: the union of ply=000 through ply=NNN.
  auto ply_name = [](int ply) {return std::format("ply={:03d}", ply);};
  auto ply_max_name = [](int ply) {return std::format("ply_max={:03d}", ply);};

  puzzle->load_or_build(ply_name(0), [&]() {return won;});
  puzzle->load_or_build(ply_max_name(0), [&]() {return won;});

  int ply_max = 100;
  std::vector<shared_dfa_ptr> positions = {won};
  std::vector<shared_dfa_ptr> positions_cumulative = {won};
  for(int ply = 1; ply <= ply_max; ++ply)
    {
      shared_dfa_ptr positions_new = puzzle->load_or_build(ply_name(ply), [&]()
      {
        return puzzle->get_moves_backward(positions.at(ply - 1));
      });
      positions.push_back(positions_new);

      shared_dfa_ptr cumulative_new = puzzle->load_or_build(ply_max_name(ply), [&]()
      {
        return DFAUtil::get_union(positions_cumulative.at(ply - 1), positions_new);
      });
      positions_cumulative.push_back(cumulative_new);

      std::cout << "PLY: " << ply << ", POSITIONS: " << positions_new->size() << ", STATES: " << positions_new->states() << ", POSITIONS/STATE: " << (positions_new->size() / double(positions_new->states())) << ", PLY_MAX POSITIONS: " << cumulative_new->size() << ", PLY_MAX STATES: " << cumulative_new->states() << std::endl;

      if((ply >= 2) && (positions_new->get_hash() == positions.at(ply - 2)->get_hash()))
        {
          break;
        }
    }

  return 0;
}
