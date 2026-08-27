// DFA.h

#ifndef DFA_H
#define DFA_H

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DFAFormat.h"
#include "MemoryMap.h"

typedef uint32_t dfa_state_t;
#define DFA_STATE_MAX UINT32_MAX

typedef std::vector<dfa_state_t> DFATransitionsStaging;

typedef std::vector<int> dfa_shape_t;

class DFAString
{
  dfa_shape_t shape;
  std::vector<int> characters;

public:

  DFAString() : shape(), characters() {};
  DFAString(const dfa_shape_t&, const std::vector<int>& characters_in);

  bool operator<(const DFAString& right) const;
  bool operator==(const DFAString& right) const;
  int operator[](int) const;

  const dfa_shape_t& get_shape() const;

  std::string to_string() const;
};

// One row of transitions, as raw bytes plus the width to read them at.
//
// A DFA being built stores transitions as uint32, while a saved DFA stores
// them at the width the format derives from the next layer's size, so this
// has to work for both. The bytes of an entry are naturally aligned -- blocks
// start on an 8 byte boundary and an entry sits at a multiple of its own
// width -- and the width is constant for a whole layer, so at() is a single
// load under a branch the predictor pins for the duration of a scan.
class DFATransitionsReference
{
  const uint8_t *row;
  int layer_shape;
  int width;

public:

  DFATransitionsReference(const uint8_t *row_in, int layer_shape_in, int width_in)
    : row(row_in),
      layer_shape(layer_shape_in),
      width(width_in)
  {
    assert(layer_shape > 0);
  }

  dfa_state_t operator[](int c) const {return at(size_t(c));}

  dfa_state_t at(size_t c) const
  {
    assert(c < size_t(layer_shape));
    return dfa_state_t(dfa_format::decode_entry(row + c * size_t(width), width));
  }

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

  dfa_state_t initial_state = ~dfa_state_t(0);
  mutable std::string name;

  std::vector<size_t> layer_sizes;

  // Staging, while the DFA is being built: ndim files of uint32 mapping
  // (state, character) -> next state. Discarded once the DFA is saved,
  // because the format needs every layer size before it can place a byte.
  mutable std::string directory;
  mutable std::vector<std::string> layer_file_names;
  mutable std::vector<MemoryMap<dfa_state_t>> layer_transitions;
  mutable bool temporary;

  // Persistent, once saved or loaded: one file in the format of
  // FORMAT-DFA.md, mapped whole.
  mutable std::string file_name;
  mutable MemoryMap<uint8_t> *file_map = 0;
  mutable dfa_format::Layout *file_layout = 0;

  // Whether this DFA's construction guarantees canonical state numbering
  // (FORMAT-DFA.md section 8). Left false unless a subclass knows better,
  // since claiming it wrongly produces a file readers will reject.
  bool canonical = false;

  mutable std::optional<std::string> hash;

  mutable double size_cache = 0.0;
  mutable bool size_cache_loaded = false;

  mutable DFALinearBound *linear_bound = 0;

  // attach_file only swaps in a new backing file; load_file also reads the
  // shape, layer sizes, initial state and flags out of its header.
  void attach_file(std::string) const;
  void close_file() const;
  void load_file(std::string);
  std::string serialize(std::string) const;

 protected:

  DFA(const dfa_shape_t&);

  virtual dfa_state_t add_state(int, const DFATransitionsStaging&);
  dfa_state_t add_state_by_function(int, std::function<dfa_state_t(int)>);
  dfa_state_t add_state_by_reference(int, const DFATransitionsReference&);

  void build_layer(int, size_t, std::function<void(dfa_state_t, dfa_state_t *)>);
  void copy_layer(int, const DFA&);
  virtual void set_initial_state(dfa_state_t);

  // Declared by whichever subclass built this DFA. See FORMAT-DFA.md section
  // 8: the flag asserts both an ordering and the minimality that makes the
  // ordering well defined.
  void set_canonical(bool);

 public:

  DFA(const dfa_shape_t&, std::string);
  virtual ~DFA() noexcept(false);

  DFAIterator cbegin() const;
  DFAIterator cend() const;

  bool contains(const DFAString&) const;

  // Recompute the digest from the mapped file, for checking it against the
  // one stored in the header and the name it is filed under.
  std::string calculate_digest() const;
  std::string get_hash() const;
  dfa_state_t get_initial_state() const;
  int get_layer_shape(int) const;
  size_t get_layer_size(int) const;
  const DFALinearBound& get_linear_bound() const;
  std::string get_name() const;
  const dfa_shape_t& get_shape() const;
  int get_shape_size() const;
  DFATransitionsReference get_transitions(int, size_t) const;

  bool is_canonical() const;
  bool is_constant(bool) const;
  bool is_linear() const;

  void mmap() const;
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

  int ndim;

  const DFA& dfa;
  std::vector<int> characters;

  // states[layer + 1] is the state reached after consuming
  // characters[layer], with states[0] the initial state. Kept across
  // operator++ so advancing does not walk the whole string again. Empty at
  // the end iterator, which has no states.
  std::vector<dfa_state_t> states;

  DFAIterator(const DFA& dfa_in, const std::vector<int>& characters_in);

public:

  DFAString operator*() const;
  DFAIterator& operator++(); // prefix ++
  bool operator<(const DFAIterator&) const;
};

typedef std::shared_ptr<const DFA> shared_dfa_ptr;

#endif
