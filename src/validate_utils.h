// validate_utils.h

#ifndef VALIDATE_UTILS_H
#define VALIDATE_UTILS_H

#include <format>
#include <vector>

#include "DFA.h"
#include "Game.h"

template <class... Args>
shared_dfa_ptr load_helper(const Game& game, const std::format_string<Args...>& name_format, Args&&... args);

bool validate_disjoint(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b);
bool validate_equal(const Game&, std::string, shared_dfa_ptr, std::string, shared_dfa_ptr);
bool validate_losing(const Game&, int, shared_dfa_ptr, shared_dfa_ptr, shared_dfa_ptr, int);
bool validate_partition(const Game& game, shared_dfa_ptr, std::vector<shared_dfa_ptr>);
bool validate_result(const Game&, int, shared_dfa_ptr, int, int);
bool validate_subset(shared_dfa_ptr, shared_dfa_ptr);
bool validate_winning(const Game&, int, shared_dfa_ptr, shared_dfa_ptr, shared_dfa_ptr, int);

#endif
