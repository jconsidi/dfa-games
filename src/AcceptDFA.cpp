// AcceptDFA.cpp

#include "AcceptDFA.h"

template <int ndim, int... shape_pack>
AcceptDFA<ndim, shape_pack...>::AcceptDFA()
{
  this->set_initial_state(1);
  this->set_name("accept");
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(AcceptDFA);
