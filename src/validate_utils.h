// validate_utils.h

#ifndef VALIDATE_UTILS_H
#define VALIDATE_UTILS_H

#include <format>
#include <vector>

#include "DFA.h"
#include "Game.h"

bool validate_equal(const Game&, std::string, shared_dfa_ptr, std::string, shared_dfa_ptr);

#endif
