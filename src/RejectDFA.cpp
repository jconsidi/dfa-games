// RejectDFA.cpp

#include "RejectDFA.h"

template <int ndim, int... shape_pack>
RejectDFA<ndim, shape_pack...>::RejectDFA()
{
  this->set_initial_state(0);
  this->set_name("reject");
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(RejectDFA);
