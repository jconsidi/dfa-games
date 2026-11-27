// InverseDFA.h

#ifndef INVERSE_DFA_H
#define INVERSE_DFA_H

#include "DFA.h"

// The complement of its input, as a view: the input's states and numbers,
// with every transition to reject going to accept and back. A constant
// input gives the opposite constant, with only the reserved rows.
class InverseDFA : public DFA
{
  shared_dfa_ptr input;

  static std::vector<size_t> view_layer_sizes(const DFA&);
  static dfa_state_t view_initial_state(const DFA&);

 public:

  InverseDFA(shared_dfa_ptr);

  virtual dfa_state_t get_transition(int, size_t, int) const;
  virtual void mmap() const;
  virtual void munmap() const;
  virtual double size() const;
};

#endif
