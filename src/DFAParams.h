// DFAParams.h

#ifndef DFA_PARAMS_H
#define DFA_PARAMS_H

#include "ChessDFAParams.h"
#include "NormalNimDFAParams.h"
#include "TestDFAParams.h"
#include "TicTacToeDFAParams.h"

#define INSTANTIATE_DFA_TEMPLATE(class_name) \
  template class class_name<CHESS_DFA_PARAMS>; \
  template class class_name<NORMALNIM1_DFA_PARAMS>; \
  template class class_name<NORMALNIM2_DFA_PARAMS>; \
  template class class_name<NORMALNIM3_DFA_PARAMS>; \
  template class class_name<NORMALNIM4_DFA_PARAMS>; \
  template class class_name<TEST1_DFA_PARAMS>; \
  template class class_name<TEST2_DFA_PARAMS>; \
  template class class_name<TEST3_DFA_PARAMS>; \
  template class class_name<TEST4_DFA_PARAMS>; \
  template class class_name<TEST5_DFA_PARAMS>; \
  template class class_name<TICTACTOE2_DFA_PARAMS>; \
  template class class_name<TICTACTOE3_DFA_PARAMS>; \
  template class class_name<TICTACTOE4_DFA_PARAMS>;

#endif
