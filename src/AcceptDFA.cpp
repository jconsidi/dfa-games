// AcceptDFA.cpp

#include "AcceptDFA.h"

AcceptDFA::AcceptDFA(const dfa_shape_t& shape_in)
  : DFA(shape_in)
{
  this->set_initial_state(1);
  this->set_name("accept");
}
