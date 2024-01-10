// validate_utils.h

#ifndef VALIDATE_UTILS_H
#define VALIDATE_UTILS_H

#include <vector>

#include "DFA.h"

bool validate_equal(shared_dfa_ptr, shared_dfa_ptr);
bool validate_partition(shared_dfa_ptr, std::vector<shared_dfa_ptr>);
bool validate_subset(shared_dfa_ptr, shared_dfa_ptr);

#endif
