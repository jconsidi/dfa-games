// AcceptDFA.cpp

#include "AcceptDFA.h"

AcceptDFA::AcceptDFA(const dfa_shape_t& shape_in)
  : DFA(shape_in)
{
  // Every layer keeps only the two reserved rows, so there is no
  // ordinary state to order and canonical numbering is vacuous.
  this->set_canonical(true);
  this->set_initial_state(1);
  this->set_name("accept");
}
