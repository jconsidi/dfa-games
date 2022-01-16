// DFA.h

#ifndef DFA_H
#define DFA_H

#include <cstdint>
#include <map>
#include <vector>

template <int ndim, int... shape_pack>
class UnionDFA;

typedef std::vector<uint64_t> DFATransitions;

struct DFATransitionsCompare
{
  bool operator() (const DFATransitions& left, const DFATransitions& right) const
  {
    int left_num_transitions = left.size();
    int right_num_transitions = right.size();

    if(left_num_transitions < right_num_transitions)
      {
	return true;
      }
    else if(left_num_transitions > right_num_transitions)
      {
	return false;
      }

    for(int i = 0; i < left_num_transitions; ++i)
      {
	if(left[i] < right[i])
	  {
	    return true;
	  }
	else if(left[i] > right[i])
	  {
	    return false;
	  }
      }

    return false;
  }
};

typedef std::map<DFATransitions, uint64_t, DFATransitionsCompare> DFATransitionsMap;

template <int ndim, int... shape_pack>
class DFA
{
  std::vector<int> shape = {};

  // ndim layers mapping (state, square contents) -> next state.
  std::vector<DFATransitions> *state_transitions = 0;

  DFATransitionsMap *state_lookup = 0;

 protected:

  DFA();

  int add_state(int, const DFATransitions&);
  int add_state(int, std::function<uint64_t(int)>);
  void add_uniform_states();

 public:

  ~DFA();

  int get_layer_shape(int) const;
  int get_layer_size(int) const;
  const DFATransitions& get_transitions(int, int) const;

  bool ready() const;
  int size() const;
  int states() const;

  friend class UnionDFA<ndim, shape_pack...>;
};

#define TEST_DFA_PARAMS 4, 1, 2, 3, 4
typedef DFA<TEST_DFA_PARAMS> TestDFA;

#endif
