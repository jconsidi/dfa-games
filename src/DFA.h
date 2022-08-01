// DFA.h

#ifndef DFA_H
#define DFA_H

#include <ctype.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "MemoryMap.h"

typedef uint32_t dfa_state_t;
typedef std::vector<dfa_state_t> DFATransitionsStaging;

template <int ndim, int... shape_pack>
class DFAString
{
  std::vector<int> characters;

public:

  DFAString(const std::vector<int>& characters_in);
  int operator[](int) const;
};

class DFATransitionsReference
{
  const MemoryMap<dfa_state_t>& layer_transitions;
  size_t offset;
  int layer_shape;

public:

  DFATransitionsReference(const MemoryMap<dfa_state_t>&, dfa_state_t, int);
  dfa_state_t operator[](int c) const {return at(c);}

  dfa_state_t at(int c) const {assert(c < layer_shape); return layer_transitions[offset + c];}
  int get_layer_shape() const {return layer_shape;}
};

template <int ndim, int... shape_pack>
class DFAIterator;

template <int ndim, int... shape_pack>
class DFA
{
  std::vector<int> shape = {};

  std::string file_prefix;
  dfa_state_t initial_state = ~dfa_state_t(0);

  // ndim layers mapping (state, square contents) -> next state.
  std::vector<std::string> layer_file_names;
  std::vector<dfa_state_t> layer_sizes;
  std::vector<MemoryMap<dfa_state_t>> layer_transitions;

  void finalize();

 protected:

  DFA();

  void emplace_transitions(int, const DFATransitionsStaging&);
  virtual void set_initial_state(dfa_state_t);

 public:

  virtual ~DFA();

  DFAIterator<ndim, shape_pack...> cbegin() const;
  DFAIterator<ndim, shape_pack...> cend() const;

  void debug_example(std::ostream&) const;

  dfa_state_t get_initial_state() const;
  int get_layer_shape(int) const;
  dfa_state_t get_layer_size(int) const;
  DFATransitionsReference get_transitions(int, dfa_state_t) const;

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
