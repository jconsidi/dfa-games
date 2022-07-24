// DFA.h

#ifndef DFA_H
#define DFA_H

#include <ctype.h>
#include <iostream>
#include <map>
#include <vector>

typedef uint32_t dfa_state_t;
typedef std::vector<dfa_state_t> DFATransitions;

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

  dfa_state_t initial_state = ~dfa_state_t(0);

  // ndim layers mapping (state, square contents) -> next state.
  std::vector<dfa_state_t> *state_transitions = 0;

 protected:

  DFA();

  void emplace_transitions(int, const DFATransitions&);
  virtual void set_initial_state(dfa_state_t);

 public:

  virtual ~DFA();

  DFAIterator<ndim, shape_pack...> cbegin() const;
  DFAIterator<ndim, shape_pack...> cend() const;

  void debug_example(std::ostream&) const;

  dfa_state_t get_initial_state() const;
  int get_layer_shape(int) const;
  dfa_state_t get_layer_size(int) const;
  DFATransitions get_transitions(int, dfa_state_t) const;

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
