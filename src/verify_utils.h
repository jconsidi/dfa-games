// verify_utils.h

#ifndef VERIFY_UTILS_H
#define VERIFY_UTILS_H

#include <cstdint>
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

#endif
