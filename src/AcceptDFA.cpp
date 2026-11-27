// AcceptDFA.cpp

#include "AcceptDFA.h"

AcceptDFA::AcceptDFA(const dfa_shape_t& shape_in)
  // Every layer keeps only the two reserved rows, so there is no ordinary
  // state to order and canonical numbering is vacuous.
  : DFA(shape_in, std::vector<size_t>(shape_in.size(), 2), 1, true)
{
  this->set_name("accept");
}
