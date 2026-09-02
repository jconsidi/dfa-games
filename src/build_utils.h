// build_utils.h

#ifndef BUILD_UTILS_H
#define BUILD_UTILS_H

#include <string>
#include <tuple>
#include <vector>

#include "DFA.h"
#include "Game.h"

typedef std::tuple<shared_dfa_ptr, shared_dfa_ptr, shared_dfa_ptr> build_triple;

void build_backward_many(const Game& game,
                         std::vector<shared_dfa_ptr> targets,
                         std::vector<std::string> winning_names,
                         std::vector<std::string> losing_names,
                         std::vector<std::string> unknown_names,
                         shared_dfa_ptr winning_base,
                         shared_dfa_ptr losing_base);

build_triple build_backward_once(const Game& game,
                                 int side_to_move,
                                 shared_dfa_ptr target_curr,
                                 std::string winning_curr_name,
                                 std::string losing_curr_name,
                                 std::string unknown_curr_name,
                                 shared_dfa_ptr winning_next,
                                 shared_dfa_ptr losing_next,
                                 shared_dfa_ptr unknown_next);

std::vector<shared_dfa_ptr> build_forward(const Game& game, int forward_ply_max);

shared_dfa_ptr build_unknown(shared_dfa_ptr target, shared_dfa_ptr winning, shared_dfa_ptr losing);

#endif
