// build_utils.h

#ifndef BUILD_UTILS_H
#define BUILD_UTILS_H

#include <string>
#include <tuple>

#include "DFA.h"
#include "Game.h"

typedef std::tuple<shared_dfa_ptr, shared_dfa_ptr, shared_dfa_ptr> build_triple;

build_triple build_backward(const Game& game,
                            int side_to_move,
                            shared_dfa_ptr target_curr,
                            std::string winning_curr_name,
                            std::string losing_curr_name,
                            std::string unknown_curr_name,
                            shared_dfa_ptr winning_next,
                            shared_dfa_ptr losing_next,
                            shared_dfa_ptr unknown_next,
                            shared_dfa_ptr winning_base,
                            shared_dfa_ptr losing_base);

#endif
