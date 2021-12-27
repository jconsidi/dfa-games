// DFA.h

#ifndef DFA_H
#define DFA_H

#include <cstdint>
#include <vector>

enum DFACharacter {DFA_BLANK, DFA_WHITE_KING, DFA_WHITE_QUEEN, DFA_WHITE_BISHOP, DFA_WHITE_KNIGHT, DFA_WHITE_ROOK, DFA_WHITE_PAWN, DFA_BLACK_KING, DFA_BLACK_QUEEN, DFA_BLACK_BISHOP, DFA_BLACK_KNIGHT, DFA_BLACK_ROOK, DFA_BLACK_PAWN, DFA_MAX};

class DFA
{
  // 63 layers mapping (state, square contents) -> next state.
  // 64th state is a bitmap of accepted square contents.
  std::vector<uint64_t> state_counts[63] = {{}};
  std::vector<uint64_t> state_transitions[63] = {{}};

 protected:

  DFA();

  int add_state(int layer, uint64_t next_states[DFA_MAX]);

 public:

  int size() const;
};

#endif
