// DFA.h

#ifndef DFA_H
#define DFA_H

#include <ctype.h>

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "MemoryMap.h"

typedef uint32_t dfa_state_t;
typedef std::vector<dfa_state_t> DFATransitionsStaging;

typedef std::vector<int> dfa_shape_t;

class DFAString
{
  dfa_shape_t shape;
  std::vector<int> characters;

public:

  DFAString(const dfa_shape_t&, const std::vector<int>& characters_in);
  int operator[](int) const;

  const dfa_shape_t& get_shape() const;

  std::string to_string() const;
};

class DFATransitionsReference
{
  const MemoryMap<dfa_state_t>& layer_transitions;
  size_t offset;
  int layer_shape;

public:

  DFATransitionsReference(const MemoryMap<dfa_state_t>&, dfa_state_t, int);
  DFATransitionsReference(const DFATransitionsReference&);
  dfa_state_t operator[](int c) const {return at(c);}

  dfa_state_t at(int c) const {assert(c < layer_shape); return layer_transitions[offset + c];}
  int get_layer_shape() const {return layer_shape;}
};

class DFAIterator;

class DFALinearBound
{
private:

  dfa_shape_t shape;
  std::vector<std::vector<bool>> bounds;

public:

  DFALinearBound(const dfa_shape_t&, const std::vector<std::vector<bool>>&);

  bool operator<=(const DFALinearBound&) const;

  bool check_bound(int, int) const;
  bool check_fixed(int, int) const;
};

class DFA
{
  dfa_shape_t shape;
  int ndim;

  mutable std::string directory;
  dfa_state_t initial_state = ~dfa_state_t(0);
  mutable std::string name;

  // ndim layers mapping (state, square contents) -> next state.
  mutable std::vector<std::string> layer_file_names;
  std::vector<dfa_state_t> layer_sizes;
  mutable std::vector<MemoryMap<dfa_state_t>> layer_transitions;

  mutable std::optional<std::string> hash;

  mutable MemoryMap<double> size_cache;
  mutable bool temporary;

  mutable DFALinearBound *linear_bound = 0;

  void finalize();

 protected:

  DFA(const dfa_shape_t&);

  virtual dfa_state_t add_state(int, const DFATransitionsStaging&);
  dfa_state_t add_state_by_function(int, std::function<dfa_state_t(int)>);
  dfa_state_t add_state_by_reference(int, const DFATransitionsReference&);

  virtual void set_initial_state(dfa_state_t);

 public:

  DFA(const dfa_shape_t&, std::string);
  virtual ~DFA() noexcept(false);

  std::string calculate_hash() const;

  DFAIterator cbegin() const;
  DFAIterator cend() const;

  bool contains(const DFAString&) const;

  std::string get_hash() const;
  dfa_state_t get_initial_state() const;
  int get_layer_shape(int) const;
  dfa_state_t get_layer_size(int) const;
  const DFALinearBound& get_linear_bound() const;
  std::string get_name() const;
  const dfa_shape_t& get_shape() const;
  int get_shape_size() const;
  DFATransitionsReference get_transitions(int, dfa_state_t) const;

  bool is_constant(bool) const;
  bool is_linear() const;

  void munmap() const;

  static std::optional<std::string> parse_hash(std::string);
  bool ready() const;
  void save(std::string) const;
  void save_by_hash() const;
  void set_name(std::string) const;
  double size() const;
  size_t states() const;
};

class DFAIterator
{
  friend class DFA;

  std::vector<int> shape;
  int ndim;

  const DFA& dfa;
  std::vector<int> characters;

  DFAIterator(const DFA& dfa_in, const std::vector<int>& characters_in);

public:

  DFAString operator*() const;
  DFAIterator& operator++(); // prefix ++
  bool operator<(const DFAIterator&) const;
};

typedef std::shared_ptr<const DFA> shared_dfa_ptr;

#endif
