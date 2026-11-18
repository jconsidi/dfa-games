// verify_proof.cpp

// Verifies a proof built by build_proof (proof_utils.h), from the initial
// position forward, one ply against the next. Each ply's set is named
// PROOF_PREFIX,ply=NNN,winning or PROOF_PREFIX,ply=NNN,losing, and that
// result is what it is checked for:
//
// * ply 0 is exactly the initial position, under one of the two names;
// * the results alternate from ply to ply;
// * at a losing ply, every move of every position is in the next ply's set;
// * at a winning ply, every position has a move into the next ply's set;
// * either way, a position whose validate_outcome is lost (losing ply) or
//   won (winning ply) needs no move, and any other outcome fails.
//
// It continues until a ply's next set is missing or empty, and verifies that
// last ply against the empty set, so each of its positions must be lost or
// won outright.

#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>

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
  shared_dfa_ptr initial_positions = game.get_positions_initial();
  if(initial_positions->size() != 1)
    {
      throw std::logic_error(std::to_string(initial_positions->size()) +
                             " initial positions, but a proof starts from exactly one");
    }

  bool initial_winning_exists = proof_ply_exists(game, get_proof_name(proof_prefix, 0, true));
  bool initial_losing_exists = proof_ply_exists(game, get_proof_name(proof_prefix, 0, false));
  if(initial_winning_exists == initial_losing_exists)
    {
      throw std::runtime_error(std::string(initial_winning_exists ? "both" : "neither") + " of " +
                               get_proof_name(proof_prefix, 0, true) + " and " +
                               get_proof_name(proof_prefix, 0, false) + " exist");
    }
  bool initial_winning = initial_winning_exists;

  std::string curr_name = get_proof_name(proof_prefix, 0, initial_winning);
  std::cout << "VERIFYING " << curr_name << " IS THE INITIAL POSITION" << std::endl;
  shared_dfa_ptr curr_positions = verify_load(game, curr_name);
  if((curr_positions->size() != 1) || !curr_positions->contains(*(initial_positions->cbegin())))
    {
      throw std::runtime_error(curr_name + " has " + std::to_string(curr_positions->size()) +
                               " positions and is not exactly the initial position");
    }
  std::cout << "VERIFIED " << curr_name << " IS THE INITIAL POSITION" << std::endl;

  double positions_verified = 0;
  int ply = 0;
  while(true)
    {
      int side_to_move = ply % 2;
      bool ply_winning = initial_winning ^ (ply % 2);

      // the next ply must have the opposite result.
      std::string mismatch_name = get_proof_name(proof_prefix, ply + 1, ply_winning);
      if(proof_ply_exists(game, mismatch_name))
        {
          throw std::runtime_error(mismatch_name + " exists, but ply " + std::to_string(ply + 1) +
                                   " must be " + (ply_winning ? "losing" : "winning") +
                                   " after a " + (ply_winning ? "winning" : "losing") + " ply");
        }

      std::string next_name = get_proof_name(proof_prefix, ply + 1, !ply_winning);
      bool last_ply = false;
      shared_dfa_ptr next_positions;
      if(proof_ply_exists(game, next_name))
        {
          next_positions = verify_load(game, next_name);
          last_ply = (next_positions->size() == 0);
        }
      else
        {
          std::cout << "NO " << next_name << ", SO PLY " << ply << " IS THE LAST" << std::endl;
          last_ply = true;
          next_name = "the empty set";
          next_positions = shared_dfa_ptr(new RejectDFA(game.get_shape()));
        }

      std::cout << "PLY " << ply << " " << (ply_winning ? "WINNING" : "LOSING") << " SIDE " << side_to_move << ": VERIFYING " << curr_name << " WITH " << next_name << std::endl;
      if(ply_winning)
        {
          verify_winning_sound(game, side_to_move, curr_positions, next_positions);
        }
      else
        {
          verify_losing_sound(game, side_to_move, curr_positions, next_positions);
        }
      positions_verified += curr_positions->size();

      if(last_ply)
        {
          break;
        }

      ++ply;
      curr_name = next_name;
      curr_positions = next_positions;
    }

  std::cout << "PROOF VERIFIED: " << proof_prefix << " plies 0-" << ply << ", " << positions_verified << " positions, initial position " << (initial_winning ? "WINNING" : "LOSING") << " for side 0" << std::endl;
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
