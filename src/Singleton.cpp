// Singleton.cpp

#include "Singleton.h"

#include "AcceptDFA.h"
#include "CheckDFA.h"
#include "PreviousDFA.h"

const DFA *Singleton::get_check(Side side_in_check)
{
  static const DFA *singletons[2] = {0, 0};
  if(singletons[side_in_check] == 0)
    {
      singletons[side_in_check] = new CheckDFA(SIDE_WHITE);
    }

  return singletons[side_in_check];
}

const DFA *Singleton::get_has_moves(Side side_to_move)
{
  static const DFA *singletons[2] = {0, 0};
  if(singletons[side_to_move] == 0)
    {
      singletons[side_to_move] = new PreviousDFA(SIDE_WHITE, AcceptDFA());
    }

  return singletons[side_to_move];
}
