// DFAParams.h

#ifndef DFA_PARAMS_H
#define DFA_PARAMS_H

#include "ChessDFAParams.h"
#include "TestDFAParams.h"
#include "TicTacToeDFAParams.h"

#define INSTANTIATE_DFA_TEMPLATE(class_name) \
  template class class_name<CHESS_DFA_PARAMS>; \
  template class class_name<TEST4_DFA_PARAMS>; \
  template class class_name<TEST5_DFA_PARAMS>; \
  template class class_name<TICTACTOE2_DFA_PARAMS>; \
  template class class_name<TICTACTOE3_DFA_PARAMS>; \
  template class class_name<TICTACTOE4_DFA_PARAMS>;

#endif
