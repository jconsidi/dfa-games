// proof_utils.h

#ifndef PROOF_UTILS_H
#define PROOF_UTILS_H

#include <string>
#include <vector>

#include "DFA.h"
#include "Game.h"

// Loads each name, printing "LOADING <name>", and throws std::logic_error
// naming the DFA when one is missing.
std::vector<shared_dfa_ptr> load_proof_inputs(const Game& game, const std::vector<std::string>& names);

// Builds and saves one proof set per ply, from ply 0 to winning.size() - 1:
// the positions reachable from the initial position when the side winning
// it keeps only moves into the next ply's losing set, and the other side
// plays every move.
//
// winning[ply] and losing[ply] are the solve's sets at that ply, with side
// ply % 2 to move. That ply's proof set is saved as proof_names[ply] with
// ",winning" or ",losing" appended, for the result it is proven to have.
void build_proof(const Game& game,
                 const std::vector<shared_dfa_ptr>& winning,
                 const std::vector<shared_dfa_ptr>& losing,
                 const std::vector<std::string>& proof_names);

#endif
