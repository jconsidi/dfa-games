// HasMovesDFA.cpp

#include "HasMovesDFA.h"

#include "AcceptDFA.h"

HasMovesDFA::HasMovesDFA(Side side_to_move)
  : PreviousDFA(side_to_move, ChessAcceptDFA())
{
}

const HasMovesDFA *HasMovesDFA::get_singleton(Side side_to_move)
{
  static const HasMovesDFA *singletons[2] = {0, 0};
  if(singletons[side_to_move] == 0)
    {
      singletons[side_to_move] = new HasMovesDFA(side_to_move);
    }

  return singletons[side_to_move];
}
