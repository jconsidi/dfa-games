// DFA.h

#ifndef DFA_H
#define DFA_H

#include <ctype.h>
#include <iostream>
#include <map>
#include <vector>

typedef uint32_t dfa_state_t;
typedef std::vector<dfa_state_t> DFATransitions;

struct DFATransitionsCompare
{
  bool operator() (const DFATransitions& left, const DFATransitions& right) const
  {
    size_t left_num_transitions = left.size();
    size_t right_num_transitions = right.size();

    if(left_num_transitions < right_num_transitions)
      {
	return true;
      }
    else if(left_num_transitions > right_num_transitions)
      {
	return false;
      }

    for(size_t i = 0; i < left_num_transitions; ++i)
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

typedef std::map<DFATransitions, dfa_state_t, DFATransitionsCompare> DFATransitionsMap;

template <int ndim, int... shape_pack>
class DFAString
{
  std::vector<int> characters;

public:

  DFAString(const std::vector<int>& characters_in);
  int operator[](int) const;
};

template <int ndim, int... shape_pack>
class DFAIterator;

template <int ndim, int... shape_pack>
class DFA
{
  std::vector<int> shape = {};

  // ndim layers mapping (state, square contents) -> next state.
  std::vector<DFATransitions> *state_transitions = 0;

  DFATransitionsMap *state_lookup = 0;

  dfa_state_t initial_state = ~dfa_state_t(0);

 protected:

  DFA();

  dfa_state_t add_state(int, const DFATransitions&);
  dfa_state_t add_state(int, std::function<dfa_state_t(int)>);
  void add_uniform_states();
  void set_initial_state(dfa_state_t);

 public:

  ~DFA();

  DFAIterator<ndim, shape_pack...> cbegin() const;
  DFAIterator<ndim, shape_pack...> cend() const;

  void debug_example(std::ostream&) const;

  dfa_state_t get_initial_state() const;
  int get_layer_shape(int) const;
  dfa_state_t get_layer_size(int) const;
  const DFATransitions& get_transitions(int, dfa_state_t) const;

  bool ready() const;
  double size() const;
  size_t states() const;
};

template <int ndim, int... shape_pack>
class DFAIterator
{
  friend class DFA<ndim, shape_pack...>;

  std::vector<int> shape;
  const DFA<ndim, shape_pack...>& dfa;
  std::vector<int> characters;

  DFAIterator(const DFA<ndim, shape_pack...>& dfa_in, const std::vector<int>& characters_in);

public:

  DFAString<ndim, shape_pack...> operator*() const;
  DFAIterator& operator++(); // prefix ++
  bool operator<(const DFAIterator&) const;
};

#endif
