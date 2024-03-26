// BinaryDFA.h

#ifndef BINARY_DFA_H
#define BINARY_DFA_H

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "BinaryFunction.h"
#include "DFA.h"

class BinaryDFA : public DFA
{
  void build_linear(const DFA&, const DFA&, const BinaryFunction&);
  void build_quadratic_mmap(const DFA&, const DFA&, const BinaryFunction&);

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

  bool operator>(const BinaryDFATransitionsHashPlusIndex& b) const
  {
    for(int i = 0; i < binary_dfa_hash_width - 1; ++i)
      {
        if(this->data[i] > b.data[i])
          {
            return true;
          }
        if(this->data[i] < b.data[i])
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

typedef std::pair<dfa_state_t, dfa_state_t> dfa_state_pair;
static_assert(sizeof(dfa_state_pair) == 2 * sizeof(dfa_state_t));

#endif
