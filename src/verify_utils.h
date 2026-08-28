// verify_utils.h

#ifndef VERIFY_UTILS_H
#define VERIFY_UTILS_H

#include <string>

#include "DFA.h"
#include "Game.h"

void verify_losing_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr winning_prev, shared_dfa_ptr lost);
void verify_losing_sound(const Game& game, int side_to_move, shared_dfa_ptr losing_curr, shared_dfa_ptr winning_prev);
void verify_lost_position(const Game& game, int side_to_move, const DFAString& position);
void verify_lost_sound(const Game& game, int side_to_move, shared_dfa_ptr positions);
int verify_parse_side_to_move(std::string);
void verify_winning_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr losing_prev, shared_dfa_ptr won);
void verify_winning_sound(const Game& game, int side_to_move, shared_dfa_ptr winning_curr, shared_dfa_ptr losing_prev);
void verify_won_position(const Game& game, int side_to_move, const DFAString& position);
void verify_won_sound(const Game& game, int side_to_move, shared_dfa_ptr positions);

#endif
