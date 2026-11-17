// proof_utils.cpp

#include "proof_utils.h"

#include <iostream>
#include <stdexcept>

#include "DFAUtil.h"

std::vector<shared_dfa_ptr> load_proof_inputs(const Game& game, const std::vector<std::string>& names)
{
  std::vector<shared_dfa_ptr> dfas;
  for(const std::string& name : names)
    {
      std::cout << "LOADING " << name << std::endl;

      shared_dfa_ptr dfa = game.load(name);
      if(!dfa)
        {
          throw std::logic_error("failed loading " + name);
        }
      dfas.push_back(dfa);
    }

  return dfas;
}

void build_proof(const Game& game,
                 const std::vector<shared_dfa_ptr>& winning,
                 const std::vector<shared_dfa_ptr>& losing,
                 const std::vector<std::string>& proof_names)
{
  if(winning.size() == 0)
    {
      throw std::invalid_argument("build_proof: no plies given");
    }
  if((losing.size() != winning.size()) || (proof_names.size() != winning.size()))
    {
      throw std::invalid_argument("build_proof: " + std::to_string(winning.size()) + " winning, " +
                                  std::to_string(losing.size()) + " losing and " +
                                  std::to_string(proof_names.size()) + " proof names given, which must match");
    }

  int ply_max = int(winning.size()) - 1;

  shared_dfa_ptr initial_positions = game.get_positions_initial();
  if(initial_positions->size() != 1)
    {
      throw std::logic_error("build_proof: " + std::to_string(initial_positions->size()) +
                             " initial positions, but a proof starts from exactly one");
    }

  DFAString initial_position = *(initial_positions->cbegin());

  // The initial position decides which side every later ply is proven for,
  // so it must be solved one way: a start in neither set has no proof.
  bool initial_winning = winning[0]->contains(initial_position);
  bool initial_losing = losing[0]->contains(initial_position);
  if(initial_winning == initial_losing)
    {
      throw std::logic_error(std::string("build_proof: the initial position is in ") +
                             (initial_winning ? "both" : "neither") +
                             " of the ply 0 winning and losing sets");
    }

  // each ply's set is saved with its result appended, so the name says what
  // verify_proof has to check it for.
  auto get_name = [&](int ply, bool ply_winning)
  {
    return proof_names[ply] + (ply_winning ? ",winning" : ",losing");
  };

  auto log_ply = [](int ply, bool ply_winning, shared_dfa_ptr positions)
  {
    std::cout << "PLY " << ply << " " << (ply_winning ? "WINNING" : "LOSING") << " " << positions->size() << " positions to prove, " << positions->states() << " states" << std::endl;
  };

  // ply 0 is the initial position.
  shared_dfa_ptr curr_positions = game.load_or_build(get_name(0, initial_winning), [&]()
  {
    return initial_positions;
  });
  log_ply(0, initial_winning, curr_positions);

  for(int ply = 0; ply < ply_max; ++ply)
    {
      int side_to_move = ply % 2;
      bool ply_winning = initial_winning ^ (ply % 2);

      curr_positions = game.load_or_build(get_name(ply + 1, !ply_winning), [&]()
      {
        shared_dfa_ptr next_positions = game.get_moves_forward(side_to_move, curr_positions);
        if(ply_winning)
          {
            // keep every move into a losing position, without choosing
            // one per position.
            return DFAUtil::get_intersection(next_positions, losing[ply + 1]);
          }
        else
          {
            // every move must be answered.
            return next_positions;
          }
      });
      log_ply(ply + 1, !ply_winning, curr_positions);
    }
}
