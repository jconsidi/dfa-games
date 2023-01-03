// RejectDFA.cpp

#include "RejectDFA.h"

RejectDFA::RejectDFA(const dfa_shape_t& shape_in)
  : DFA(shape_in)
{
  this->set_initial_state(0);
  this->set_name("reject");
}
