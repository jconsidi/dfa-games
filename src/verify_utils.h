// verify_utils.h

#ifndef VERIFY_UTILS_H
#define VERIFY_UTILS_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "DFA.h"
#include "Game.h"

void verify_losing_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr winning_prev);
void verify_losing_sound(const Game& game, int side_to_move, shared_dfa_ptr losing_curr, shared_dfa_ptr winning_prev);
void verify_losing_sound(const Game& game, int side_to_move, std::string losing_curr_name, std::string winning_prev_name);
void verify_lost_position(const Game& game, int side_to_move, const DFAString& position);
void verify_lost_sound(const Game& game, int side_to_move, shared_dfa_ptr positions);
void verify_lost_sound(const Game& game, int side_to_move, std::string lost_name);
int verify_parse_side_to_move(std::string);

// Checks that parts partition whole: every position of whole is in exactly
// one part, and every position of each part is in whole. Throws
// std::runtime_error on the first position found otherwise, or if a DFA's
// position count does not match its enumeration. Returns the positions in
// whole.
uint64_t verify_partition(const Game& game, shared_dfa_ptr whole, const std::vector<shared_dfa_ptr>& parts);
uint64_t verify_partition(const Game& game, std::string whole_name, const std::vector<std::string>& part_names);

// Loads dfa_name, throwing std::runtime_error naming the game and DFA when it
// is missing or fails to load.
shared_dfa_ptr verify_load(const Game& game, std::string dfa_name);

void verify_winning_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr losing_prev);
void verify_winning_sound(const Game& game, int side_to_move, shared_dfa_ptr winning_curr, shared_dfa_ptr losing_prev);
void verify_winning_sound(const Game& game, int side_to_move, std::string winning_curr_name, std::string losing_prev_name);
void verify_won_position(const Game& game, int side_to_move, const DFAString& position);
void verify_won_sound(const Game& game, int side_to_move, shared_dfa_ptr positions);
void verify_won_sound(const Game& game, int side_to_move, std::string won_name);

// Checks next is exactly the positions reached by one move from curr, side
// to move to play: every move of every position of curr is in next, and
// every position of next is some such move. A position whose game is over
// has no moves. Each position of next is numbered by its rank in DFA order,
// and a bit per rank marks it reached, so the check needs memory for next's
// DFA and one bit per position of next, not for the moves. Throws on the
// first failure, naming the position. Returns the positions in next.
uint64_t verify_forward_exact(const Game& game, int side_to_move, shared_dfa_ptr curr, shared_dfa_ptr next);

// One ply of a solve: domain is every position the solve covers at that ply,
// and winning, losing and unknown should partition it.
struct VerifyPlySets
{
  shared_dfa_ptr domain;
  shared_dfa_ptr winning;
  shared_dfa_ptr losing;
  shared_dfa_ptr unknown;
};

struct VerifyPlyStats
{
  int ply;
  uint64_t positions;
  uint64_t winning;
  uint64_t losing;
  uint64_t unknown;
  double seconds;
};

enum class VerifyClosure
{
  // Every move from a ply's domain is in the next ply's domain.
  every_move,
  // Only a losing position's moves are, since a proof keeps just the
  // winning moves of a winning position.
  losing_moves_only
};

// Verifies a solve from ply_max down to ply 0, throwing on the first
// failure. sets(p) is called exactly once per ply, in decreasing order of p,
// and its DFAs are used for both ply p's own checks and ply p - 1's lookups.
// Ply p has side (side_to_move_at_ply_0 + p) % 2 to move.
//
// At ply_max, every winning position is won, every losing position lost,
// and every unknown position tied or not over. At each ply p below it,
// against ply p + 1:
//
// * losing: lost, or at least one move and every move in the next winning
//   set;
// * unknown: tied, or not over with every move in the next winning or
//   unknown set and at least one in the next unknown set;
// * winning: won, or a move into the next losing set; with
//   VerifyClosure::every_move, also every move in the next domain.
//
// Ply p's checks run before ply p's partition check, so they look up into
// ply p + 1's DFAs right after its partition check scanned them. Finally,
// ply 0's domain must contain the initial position, when ply 0 has side 0
// to move as the initial position does. Returns per-ply stats, ply 0 first,
// and prints them as a table with the initial position's result.
std::vector<VerifyPlyStats> verify_solve(const Game& game, int ply_max, int side_to_move_at_ply_0,
                                         const std::function<VerifyPlySets(int)>& sets,
                                         VerifyClosure closure);

#endif
