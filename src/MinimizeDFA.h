// MinimizeDFA.h

#ifndef MINIMIZE_DFA_H
#define MINIMIZE_DFA_H

#include "DFA.h"

class MinimizeDFA : public DFA
{
 private:

  MemoryMap<dfa_state_t> minimize_layer(const DFA&, int, const MemoryMap<dfa_state_t>&);

 public:

  MinimizeDFA(const DFA&);
};

const int minimize_dfa_hash_bytes = 16;
const int minimize_dfa_hash_width = minimize_dfa_hash_bytes / sizeof(dfa_state_t);
struct MinimizeDFATransitionsHashPlusIndex
{
  dfa_state_t data[minimize_dfa_hash_width];

  bool operator<(const MinimizeDFATransitionsHashPlusIndex& b) const
  {
    for(int i = 0; i < minimize_dfa_hash_width - 1; ++i)
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

  bool operator==(const MinimizeDFATransitionsHashPlusIndex& b) const
  {
    for(int i = 0; i < minimize_dfa_hash_width - 1; ++i)
      {
        if(this->data[i] != b.data[i])
          {
            return false;
          }
      }

    return true;
  };

  dfa_state_t get_curr_state() const
  {
    return data[minimize_dfa_hash_width - 1];
  }
};
static_assert(sizeof(MinimizeDFATransitionsHashPlusIndex) == minimize_dfa_hash_bytes);

#endif
