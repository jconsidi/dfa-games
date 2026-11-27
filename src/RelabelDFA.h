// RelabelDFA.h

#ifndef RELABEL_DFA_H
#define RELABEL_DFA_H

#include <vector>

#include "DFA.h"

// A view over a base DFA with the same states, whose rows are the base's
// with characters moved: sources[layer] is empty when the layer is
// unchanged, or gives for each character the base character whose
// transition it takes, or -1 for reject.
//
// Each layer's sources must be distinct apart from -1, and a subclass
// must make sure they cover every character the base uses there, so the
// view is a bijection on the base's language and has the base's size.
class RelabelDFA : public DFA
{
  shared_dfa_ptr base;
  std::vector<std::vector<int>> sources;

  static std::vector<size_t> view_layer_sizes(const DFA&);

 protected:

  RelabelDFA(shared_dfa_ptr, const std::vector<std::vector<int>>&);

 public:

  virtual dfa_state_t get_transition(int, size_t, int) const;
  virtual void mmap() const;
  virtual void munmap() const;
  virtual double size() const;
};

#endif
