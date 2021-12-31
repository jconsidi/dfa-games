// prototype.cpp

#include <iostream>
#include <vector>

#include "chess.h"

#include "CheckDFA.h"
#include "DFA.h"
#include "ForcedDFA.h"
#include "HasMovesDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "PreviousDFA.h"
#include "UnionDFA.h"

const DFA *log_dfa(std::string dfa_name, const DFA *dfa)
{
  std::cout << dfa_name << ": " << dfa->states() << " states, " << dfa->size() << " positions" << std::endl;
  return dfa;
}

int main()
{
  const DFA *white_in_check = log_dfa("white in check", CheckDFA::get_singleton(SIDE_WHITE));

  const DFA *white_has_moves = log_dfa("white has moves", HasMovesDFA::get_singleton(SIDE_WHITE));
  const DFA *white_has_no_moves = log_dfa("white has no moves", new InverseDFA(*white_has_moves));

  const DFA *white_in_checkmate = log_dfa("white in checkmate", new IntersectionDFA(*white_in_check, *white_has_no_moves));

  const DFA *white_loses_in_0 = white_in_checkmate;

  const DFA *black_wins_in_1 = log_dfa("black wins in 1 move", new PreviousDFA(SIDE_BLACK, *white_loses_in_0));
  const DFA *white_loses_in_1 = log_dfa("white loses in 1 move", new ForcedDFA(SIDE_WHITE, *black_wins_in_1));

  const DFA *black_wins_in_2 = log_dfa("white wins in 2 moves", new PreviousDFA(SIDE_BLACK, *white_loses_in_1));
  const DFA *black_wins_in_1_2 = log_dfa("black wins in 1-2 moves", new UnionDFA(*black_wins_in_1, *black_wins_in_2));
  const DFA *white_loses_in_1_2 = log_dfa("white loses in 1-2 moves", new ForcedDFA(SIDE_WHITE, *black_wins_in_1_2));

  const DFA *black_wins_in_2_3 = log_dfa("black wins in 2-3 moves", new PreviousDFA(SIDE_BLACK, *white_loses_in_1_2));
  const DFA *black_wins_in_1_3 = log_dfa("black wins in 1-3 moves", new UnionDFA(*black_wins_in_1, *black_wins_in_2_3));
  const DFA *white_loses_in_1_3 = log_dfa("white loses in 1-3 moves", new ForcedDFA(SIDE_WHITE, *black_wins_in_1_3));

  const DFA *black_wins_in_2_4 = log_dfa("black wins in 2-4 moves", new PreviousDFA(SIDE_BLACK, *white_loses_in_1_3));
  const DFA *black_wins_in_1_4 = log_dfa("black wins in 1-4 moves", new UnionDFA(*black_wins_in_1, *black_wins_in_2_4));
  const DFA *white_loses_in_1_4 = log_dfa("white loses in 1-4", new ForcedDFA(SIDE_WHITE, *black_wins_in_1_4));
  assert(white_loses_in_1_4);

  return 0;
}
