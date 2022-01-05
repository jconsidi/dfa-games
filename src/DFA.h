// DFA.h

#ifndef DFA_H
#define DFA_H

#include <cstdint>
#include <map>
#include <vector>

enum DFACharacter {DFA_BLANK, DFA_WHITE_KING, DFA_WHITE_QUEEN, DFA_WHITE_BISHOP, DFA_WHITE_KNIGHT, DFA_WHITE_ROOK, DFA_WHITE_PAWN, DFA_BLACK_KING, DFA_BLACK_QUEEN, DFA_BLACK_BISHOP, DFA_BLACK_KNIGHT, DFA_BLACK_ROOK, DFA_BLACK_PAWN, DFA_MAX};

class UnionDFA;

struct DFAState
{
  uint64_t transitions[DFA_MAX];

  class RewriteDFA;
  inline DFAState(const uint64_t transitions_in[DFA_MAX])
  {
    for(int i = 0; i < DFA_MAX; ++i)
      {
	transitions[i] = transitions_in[i];
      }
  }
};

struct DFAStateCompare
{
  bool operator() (const DFAState& left, const DFAState& right) const
  {
    for(int i = 0; i < DFA_MAX; ++i)
      {
	if(left.transitions[i] < right.transitions[i])
	  {
	    return true;
	  }
	else if(left.transitions[i] > right.transitions[i])
	  {
	    return false;
	  }
      }

    return false;
  }
};

typedef std::map<DFAState, uint64_t, DFAStateCompare> DFAStateMap;

class DFA
{
  // 63 layers mapping (state, square contents) -> next state.
  // 64th state is a bitmap of accepted square contents.
  std::vector<DFAState> state_transitions[64] = {{}};

  DFAStateMap *state_lookup;

 protected:

  DFA();

  int add_state(int layer, const uint64_t next_states[DFA_MAX]);
  void add_uniform_states();

 public:

  ~DFA();

  typedef uint64_t size_type;

  int get_layer_size(int) const;
  const DFAState& get_state(int, int) const;

  bool ready() const;
  size_type size() const;
  size_type states() const;

  friend class UnionDFA;
};

#endif
