// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "BinaryFunction.h"
#include "DFA.h"

class dfa_state_pair_t
{
  dfa_state_t left;
  dfa_state_t right;

public:

  dfa_state_pair_t()
    : left(0),
      right(0)
  {
  }

  dfa_state_pair_t(dfa_state_t left_in, dfa_state_t right_in)
    : left(left_in),
      right(right_in)
  {
  }

  dfa_state_pair_t(const dfa_state_pair_t& pair_in)
    : left(pair_in.left),
      right(pair_in.right)
  {
  }

  dfa_state_pair_t(dfa_state_pair_t&& pair_in)
    : left(pair_in.left),
      right(pair_in.right)
  {
  }

  bool operator<(const dfa_state_pair_t& right_in) const
  {
    return ((left < right_in.left) ||
            ((left == right_in.left) && (right < right_in.right)));
  }

  bool operator<=(const dfa_state_pair_t& right_in) const
  {
    return ((left < right_in.left) ||
            ((left == right_in.left) && (right <= right_in.right)));
  }

  dfa_state_pair_t& operator=(const dfa_state_pair_t& pair_in)
  {
    left = pair_in.left;
    right = pair_in.right;
    return *this;
  }

  dfa_state_pair_t& operator=(dfa_state_pair_t&& pair_in)
  {
    left = pair_in.left;
    right = pair_in.right;
    return *this;
  }

  bool operator==(const dfa_state_pair_t& right_in) const
  {
    return ((left == right_in.left) &&
            (right == right_in.right));
  }

  void check(size_t left_size, size_t right_size) const
  {
    assert(left < left_size);
    assert(right < right_size);
  }

  dfa_state_t get_left_state() const
  {
    return left;
  }

  dfa_state_t get_right_state() const
  {
    return right;
  }
};
static_assert(sizeof(dfa_state_pair_t) == 8);

class BinaryDFA : public DFA
{
  BinaryFunction leaf_func;

  // Set when a layer's transitions were too wide to be the sort key
  // directly and had to be hashed. Sorting by hash renumbers states in an
  // arbitrary order, which is what costs this DFA its canonical numbering.
  bool hashed_any_layer = false;

  // This instance's own staging directory under binarydfa/, created by
  // create_binary_directory below. Empty until then -- most BinaryDFA
  // instances (constant, sink, and other early-return cases in the public
  // constructor) never touch it at all.
  std::string binary_directory;

  void build_linear(const DFA&, const DFA&);

  void build_quadratic(const DFA&, const DFA&);
  MemoryMap<dfa_state_pair_t> build_quadratic_transition_pairs(const DFA&, const DFA&, int layer);

  std::function<bool(dfa_state_t, dfa_state_t)> get_filter_func() const;
  std::function<dfa_state_t(dfa_state_t, dfa_state_t)> get_shortcircuit_func() const;

  std::string binary_dir() const;
  std::string binary_build_file_prefix(int layer) const;
  std::string memory_map_name(int layer, std::string suffix) const;
  template<class T>
  MemoryMap<T> memory_map_helper(int layer, std::string suffix, size_t size_in) const;

protected:

  BinaryDFA(const dfa_shape_t&, const BinaryFunction&);

  // binarydfa/ staging is scoped per BinaryDFA instance -- bare pid, like
  // DFA's own build/ staging -- since two constructions, even in the same
  // process, must never share one: they would race through the same fixed
  // filenames (transitions, layer=00-pairs, ...) and corrupt each other.
  // binary_build_in_progress (BinaryDFA.cpp) is what makes that an asserted
  // invariant rather than just an observed one.
  //
  // build_linear and build_quadratic each call create_binary_directory
  // themselves, once, before using binary_dir(). BinaryRestartDFA resumes
  // by calling build_quadratic_forward/backward directly instead, so it
  // must call create_binary_directory itself first.
  //
  // The returned guard removes the directory, and anything still in it,
  // when it goes out of scope -- covering both a normal return and an
  // exception unwinding through it, the same as DFA's own destructor does
  // for its build/ staging, but scoped to one call instead of the whole
  // object, since binarydfa/ is needed only while inside one of these.
  class DirectoryGuard
  {
    std::string directory;

  public:

    explicit DirectoryGuard(std::string);
    ~DirectoryGuard() noexcept(false);

    DirectoryGuard(const DirectoryGuard&) = delete;
    DirectoryGuard& operator=(const DirectoryGuard&) = delete;
  };
  DirectoryGuard create_binary_directory();

  dfa_state_t build_quadratic_backward(const DFA&, const DFA&, int);
  MemoryMap<dfa_state_t> build_quadratic_backward_layer(const DFA&, const DFA&, int, const MemoryMap<dfa_state_t>&);
  template <bool wide>
  MemoryMap<dfa_state_t> build_quadratic_backward_layer(const DFA&, const DFA&, int, size_t, const MemoryMap<dfa_state_t>&);
  int build_quadratic_forward(const DFA&, const DFA&);
  int build_quadratic_forward(const DFA&, const DFA&, int);
  MemoryMap<dfa_state_pair_t> build_quadratic_forward_layer(const DFA&, const DFA& right_in, int layer);

  MemoryMap<dfa_state_pair_t> build_quadratic_read_pairs(int layer);

public:

  BinaryDFA(const DFA&, const DFA&, const BinaryFunction&);
};

const int binary_dfa_hash_bytes = 16;
const int binary_dfa_hash_width = binary_dfa_hash_bytes / sizeof(dfa_state_t);

// Sort record for one pair in build_quadratic_backward_layer: a sort key
// (the pair's transitions, or a hash of them) plus the pair's rank, in 16
// bytes either way.
//
// Narrow (wide = false), for layers whose pair ranks fit in 32 bits: the key
// is words 0..2 and the rank is word 3.
//
// Wide (wide = true), for larger layers: the rank is 40 bits, its high 8 bits
// in the low byte of word 2, so the key is words 0, 1 and the high 24 bits of
// word 2. Truncating word 2 from the bottom keeps the two orders consistent:
// keys that differ in the wide key compare the same way in the narrow one,
// and keys that tie in the wide key either have identical transitions or are
// a hash collision, which the sort hash check rejects. So a hashed layer
// numbers its states identically in both modes. What wide mode gives up is
// one direct transition: three 32-bit transitions no longer fit in 88 bits,
// so a shape 3 layer is hashed instead, and loses canonical numbering.
template <bool wide>
struct BinaryDFATransitionsHashPlusIndex
{
  static constexpr int key_words = binary_dfa_hash_width - 1;
  // Layers with at most this many transitions use them as the key directly.
  static constexpr int direct_max = wide ? key_words - 1 : key_words;
  static constexpr size_t pair_rank_max = wide ? (size_t(1) << 40) - 1 : size_t(UINT32_MAX);

  dfa_state_t data[binary_dfa_hash_width];

  dfa_state_t get_key_word(int i) const
  {
    if(wide && (i == key_words - 1))
      {
        return data[i] >> 8;
      }

    return data[i];
  }

  bool operator<(const BinaryDFATransitionsHashPlusIndex& b) const
  {
    for(int i = 0; i < key_words; ++i)
      {
        if(this->get_key_word(i) < b.get_key_word(i))
          {
            return true;
          }
        if(this->get_key_word(i) > b.get_key_word(i))
          {
            return false;
          }
      }

    return false;
  };

  bool operator==(const BinaryDFATransitionsHashPlusIndex& b) const
  {
    for(int i = 0; i < key_words; ++i)
      {
        if(this->get_key_word(i) != b.get_key_word(i))
          {
            return false;
          }
      }

    return true;
  };

  size_t get_pair_rank() const
  {
    size_t pair_rank = data[binary_dfa_hash_width - 1];
    if(wide)
      {
        pair_rank |= size_t(data[key_words - 1] & 0xFF) << 32;
      }

    return pair_rank;
  }

  // key_in holds key_words words. In wide mode the low byte of the last one
  // is overwritten by the rank.
  void set(const dfa_state_t *key_in, size_t pair_rank)
  {
    assert(pair_rank <= pair_rank_max);

    for(int i = 0; i < key_words; ++i)
      {
        data[i] = key_in[i];
      }
    data[binary_dfa_hash_width - 1] = dfa_state_t(pair_rank);
    if(wide)
      {
        data[key_words - 1] = (data[key_words - 1] & ~dfa_state_t(0xFF)) | dfa_state_t(pair_rank >> 32);
      }

    assert(get_pair_rank() == pair_rank);
  }
};
static_assert(sizeof(BinaryDFATransitionsHashPlusIndex<false>) == binary_dfa_hash_bytes);
static_assert(sizeof(BinaryDFATransitionsHashPlusIndex<true>) == binary_dfa_hash_bytes);
static_assert(binary_dfa_hash_width == 4);

// Largest pair count build_quadratic_backward_layer handles in narrow mode.
// Production value is UINT32_MAX. Tests lower it to run the wide path on
// small inputs, where it must produce the same DFA.
extern size_t binary_dfa_narrow_pair_count_max;

#endif
