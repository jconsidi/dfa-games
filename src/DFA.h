// DFA.h

#ifndef DFA_H
#define DFA_H

#include <cstdint>
#include <vector>

enum DFACharacter {DFA_BLANK, DFA_WHITE_KING, DFA_WHITE_QUEEN, DFA_WHITE_BISHOP, DFA_WHITE_KNIGHT, DFA_WHITE_ROOK, DFA_WHITE_PAWN, DFA_BLACK_KING, DFA_BLACK_QUEEN, DFA_BLACK_BISHOP, DFA_BLACK_KNIGHT, DFA_BLACK_ROOK, DFA_BLACK_PAWN, DFA_MAX};
#define DFA_MASK_ACCEPT_ALL ((1ULL << DFA_MAX) - 1)
#define DFA_MASK_REJECT_ALL (0ULL)

class BinaryDFA;
class InverseDFA;

struct DFAState
{
  uint64_t transitions[DFA_MAX];

  inline DFAState(const uint64_t transitions_in[DFA_MAX])
  {
    for(int i = 0; i < DFA_MAX; ++i)
      {
	transitions[i] = transitions_in[i];
      }
  }
};

class DFA
{
  // 63 layers mapping (state, square contents) -> next state.
  // 64th state is a bitmap of accepted square contents.
  std::vector<uint64_t> state_counts[63] = {{}};
  std::vector<DFAState> state_transitions[63] = {{}};

 protected:

  DFA();

  int add_state(int layer, const uint64_t next_states[DFA_MAX]);

 public:

  typedef uint64_t size_type;

  void debug_counts(std::string) const;

  bool ready() const;
  size_type size() const;
  size_type states() const;

  friend class BinaryDFA;
  friend class InverseDFA;
};

#endif
