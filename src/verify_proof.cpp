// verify_proof.cpp

// Verifies a proof built by build_proof (proof_utils.h) with verify_solve
// (verify_utils.h). Each ply's set is named PROOF_PREFIX,ply=NNN,winning or
// PROOF_PREFIX,ply=NNN,losing, and that result is what it is checked for.
// From the names alone, before any DFA is loaded:
//
// * exactly one of the two ply 0 names exists, giving the initial result;
// * the results alternate from ply to ply, with no file under the other
//   result's name;
// * the last ply is the last one whose next ply has no file.
//
// Each ply's set is then both its domain and its winning or losing set,
// with the other two sets empty, for verify_solve with
// VerifyClosure::losing_moves_only: a winning position needs a move into
// the next ply's set, and every move of a losing position must be in it.
// The last ply's positions must be won or lost outright. Finally ply 0 must
// be exactly the initial position.

#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "RejectDFA.h"
#include "ScratchConfig.h"
#include "game_utils.h"
#include "verify_utils.h"

std::string get_proof_name(std::string proof_prefix, int ply, bool ply_winning)
{
  return std::format("{:s},ply={:03d},{:s}", proof_prefix, ply, ply_winning ? "winning" : "losing");
}

bool proof_ply_exists(const Game& game, std::string dfa_name)
{
  // symlink_status so a dangling link counts as present, and then fails
  // loudly in verify_load instead of ending the proof early.
  std::filesystem::path path = std::filesystem::path(ScratchConfig::get_archive_dir()) / game.get_name() / dfa_name;
  return std::filesystem::exists(std::filesystem::symlink_status(path));
}

void verify_proof(const Game& game, std::string proof_prefix)
{
  bool initial_winning_exists = proof_ply_exists(game, get_proof_name(proof_prefix, 0, true));
  bool initial_losing_exists = proof_ply_exists(game, get_proof_name(proof_prefix, 0, false));
  if(initial_winning_exists == initial_losing_exists)
    {
      throw std::runtime_error(std::string(initial_winning_exists ? "both" : "neither") + " of " +
                               get_proof_name(proof_prefix, 0, true) + " and " +
                               get_proof_name(proof_prefix, 0, false) + " exist");
    }
  bool initial_winning = initial_winning_exists;

  auto ply_winning = [&](int ply)
  {
    return initial_winning ^ bool(ply % 2);
  };

  // the last ply, from the names alone
  int ply_max = 0;
  while(true)
    {
      // the next ply must have the opposite result.
      std::string mismatch_name = get_proof_name(proof_prefix, ply_max + 1, ply_winning(ply_max));
      if(proof_ply_exists(game, mismatch_name))
        {
          throw std::runtime_error(mismatch_name + " exists, but ply " + std::to_string(ply_max + 1) +
                                   " must be " + (ply_winning(ply_max) ? "losing" : "winning") +
                                   " after a " + (ply_winning(ply_max) ? "winning" : "losing") + " ply");
        }

      std::string next_name = get_proof_name(proof_prefix, ply_max + 1, ply_winning(ply_max + 1));
      if(!proof_ply_exists(game, next_name))
        {
          std::cout << "NO " << next_name << ", SO PLY " << ply_max << " IS THE LAST" << std::endl;
          break;
        }

      ++ply_max;
    }

  shared_dfa_ptr empty(new RejectDFA(game.get_shape()));
  std::vector<VerifyPlyStats> stats = verify_solve(game, ply_max, [&](int ply)
  {
    std::string name = get_proof_name(proof_prefix, ply, ply_winning(ply));
    std::cout << "PLY " << ply << " " << (ply_winning(ply) ? "WINNING" : "LOSING") << ": " << name << std::endl;
    shared_dfa_ptr positions = verify_load(game, name);
    return VerifyPlySets{positions,
                         ply_winning(ply) ? positions : empty,
                         ply_winning(ply) ? empty : positions,
                         empty};
  }, VerifyClosure::losing_moves_only);

  // verify_solve checked ply 0 holds the initial position; a proof starts
  // from it alone.
  if(stats.at(0).positions != 1)
    {
      throw std::runtime_error(get_proof_name(proof_prefix, 0, initial_winning) + " has " +
                               std::to_string(stats.at(0).positions) +
                               " positions and is not exactly the initial position");
    }

  uint64_t positions_verified = 0;
  for(const VerifyPlyStats& ply_stats : stats)
    {
      positions_verified += ply_stats.positions;
    }

  std::cout << "PROOF VERIFIED: " << proof_prefix << " plies 0-" << ply_max << ", " << positions_verified << " positions, initial position " << (initial_winning ? "WINNING" : "LOSING") << " for side 0" << std::endl;
}

int main(int argc, char **argv)
{
  if(argc != 3)
    {
      std::cerr << "usage: verify_proof GAME PROOF_PREFIX" << std::endl;
      std::cerr << "example: verify_proof amazons_4x5 proof_bound_backward,forward_ply_max=012,backward_ply_max=000" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  std::string proof_prefix(argv[2]);

  verify_proof(*game, proof_prefix);

  return 0;
}
