// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

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
  int build_quadratic_forward(const DFA&, const DFA&);
  int build_quadratic_forward(const DFA&, const DFA&, int);
  MemoryMap<dfa_state_pair_t> build_quadratic_forward_layer(const DFA&, const DFA& right_in, int layer);

  MemoryMap<dfa_state_pair_t> build_quadratic_read_pairs(int layer);

private:

  // n-ary union/intersection construction, used by the public vector
  // constructor below. Generalizes the pairwise machinery above from
  // dfa_state_pair_t (width 2) to a flat row of dfa_state_t of width
  // dfas_in.size(), one state per input DFA. See BinaryDFA.cpp for the
  // design notes on why this can always claim canonical numbering, unlike
  // build_quadratic. Private, unlike the build_quadratic_* family above:
  // nothing outside the vector constructor itself (in particular, no
  // BinaryRestartDFA-style resume) needs these.
  bool nary_is_union = false;

  void build_nary(const std::vector<shared_dfa_ptr>&);
  int build_nary_forward(const std::vector<shared_dfa_ptr>&);
  MemoryMap<dfa_state_t> build_nary_forward_layer(const std::vector<shared_dfa_ptr>&, int layer);
  dfa_state_t build_nary_backward(const std::vector<shared_dfa_ptr>&, int backward_layers);
  MemoryMap<dfa_state_t> build_nary_backward_layer(const std::vector<shared_dfa_ptr>&, int layer, const MemoryMap<dfa_state_t>&);
  MemoryMap<dfa_state_t> build_nary_transition_tuples(const std::vector<shared_dfa_ptr>&, int layer);
  MemoryMap<dfa_state_t> build_nary_read_tuples(int layer) const;
  std::string nary_tuples_name(int layer) const;

  // True iff every element of a width-wide tuple can be resolved to a state
  // in the terminal pseudo-layer ({0, 1}) without looking at the next layer:
  // either every element is already itself 0 or 1, or at least one equals
  // the sink value for this DFA's function (1 for union, 0 for
  // intersection). Mirrors get_filter_func's two-input version.
  bool nary_filter(const dfa_state_t *tuple, size_t width) const;
  // Valid only where nary_filter is true. Mirrors get_shortcircuit_func.
  dfa_state_t nary_shortcircuit(const dfa_state_t *tuple, size_t width) const;

public:

  BinaryDFA(const DFA&, const DFA&, const BinaryFunction&);

  // n-ary union (is_union_in true) or intersection (false) of dfas_in, all
  // sharing one shape. Correct for any dfas_in.size() >= 1, including
  // duplicates (union/intersection are idempotent), but callers combining
  // many DFAs should still dedupe identical hashes and pull out any
  // constant/absorbing element themselves first -- this constructor makes
  // no attempt to special case those, unlike the two-input constructor
  // above.
  BinaryDFA(const std::vector<shared_dfa_ptr>& dfas_in, bool is_union_in);
};

const int binary_dfa_hash_bytes = 16;
const int binary_dfa_hash_width = binary_dfa_hash_bytes / sizeof(dfa_state_t);
struct BinaryDFATransitionsHashPlusIndex
{
  dfa_state_t data[binary_dfa_hash_width];

  bool operator<(const BinaryDFATransitionsHashPlusIndex& b) const
  {
    for(int i = 0; i < binary_dfa_hash_width - 1; ++i)
      {
        if(this->data[i] < b.data[i])
          {
            return true;
          }
        if(this->data[i] > b.data[i])
          {
            return false;
          }
      }

    return false;
  };

  bool operator==(const BinaryDFATransitionsHashPlusIndex& b) const
  {
    for(int i = 0; i < binary_dfa_hash_width - 1; ++i)
      {
        if(this->data[i] != b.data[i])
          {
            return false;
          }
      }

    return true;
  };

  dfa_state_t get_pair_rank() const
  {
    return data[binary_dfa_hash_width - 1];
  }
};
static_assert(sizeof(BinaryDFATransitionsHashPlusIndex) == binary_dfa_hash_bytes);

#endif
