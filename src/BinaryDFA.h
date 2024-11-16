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
  uint64_t pair;

public:

#if 1
  dfa_state_pair_t()
    : pair(0)
  {
  }
#endif

  dfa_state_pair_t(const uint64_t& pair_in)
    : pair(pair_in)
  {
  }

  dfa_state_pair_t(const dfa_state_pair_t& pair_in)
    : pair(pair_in.pair)
  {
  }

  dfa_state_pair_t(dfa_state_pair_t&& pair_in)
    : pair(pair_in.pair)
  {
  }

  dfa_state_pair_t& operator++()
  {
    ++pair;
    return *this;
  }

  size_t operator-(const dfa_state_pair_t& right_in) const
  {
    assert(pair >= right_in.pair);
    return size_t(pair - right_in.pair);
  }

  bool operator<(const dfa_state_pair_t& right_in) const
  {
    return pair < right_in.pair;
  }

  bool operator<=(const dfa_state_pair_t& right_in) const
  {
    return pair <= right_in.pair;
  }

  dfa_state_pair_t& operator=(uint64_t pair_in)
  {
    pair = pair_in;
    return *this;
  }

  dfa_state_pair_t& operator=(const dfa_state_pair_t& pair_in)
  {
    pair = pair_in.pair;
    return *this;
  }

  dfa_state_pair_t& operator=(dfa_state_pair_t&& pair_in)
  {
    pair = pair_in.pair;
    return *this;
  }

  bool operator==(const dfa_state_pair_t& right_in) const
  {
    return pair == right_in.pair;
  }

  dfa_state_t get_left_state(dfa_state_t right_size) const
  {
    return pair / uint64_t(right_size);
  }

  dfa_state_t get_right_state(dfa_state_t right_size) const
  {
    return pair % uint64_t(right_size);
  }
};
static_assert(sizeof(dfa_state_pair_t) == sizeof(uint64_t));

class BinaryDFA : public DFA
{
  BinaryFunction leaf_func;

  void build_linear(const DFA&, const DFA&);

  void build_quadratic(const DFA&, const DFA&);
  MemoryMap<dfa_state_pair_t> build_quadratic_read_pairs(int layer);
  MemoryMap<dfa_state_pair_t> build_quadratic_transition_pairs(const DFA&, const DFA&, int layer);

  std::function<bool(dfa_state_t, dfa_state_t)> get_filter_func() const;
  std::function<dfa_state_t(dfa_state_t, dfa_state_t)> get_shortcircuit_func() const;

protected:

  BinaryDFA(const dfa_shape_t&, const BinaryFunction&);

  void build_quadratic_backward(const DFA&, const DFA&, int);
  MemoryMap<dfa_state_t> build_quadratic_backward_layer(const DFA&, const DFA&, int, const MemoryMap<dfa_state_t>&);

public:

  BinaryDFA(const DFA&, const DFA&, const BinaryFunction&);
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
