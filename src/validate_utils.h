// validate_utils.h

#ifndef VALIDATE_UTILS_H
#define VALIDATE_UTILS_H

#include <vector>

#include "DFA.h"
#include "Game.h"

bool validate_disjoint(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b);
bool validate_equal(shared_dfa_ptr, shared_dfa_ptr);
bool validate_losing(const Game&, int, shared_dfa_ptr, shared_dfa_ptr, shared_dfa_ptr, int);
bool validate_partition(shared_dfa_ptr, std::vector<shared_dfa_ptr>);
bool validate_result(const Game&, int, shared_dfa_ptr, int, int);
bool validate_subset(shared_dfa_ptr, shared_dfa_ptr);
bool validate_winning(const Game&, int, shared_dfa_ptr, shared_dfa_ptr, shared_dfa_ptr, int);

#endif
