// TableDFA.h

#ifndef TABLE_DFA_H
#define TABLE_DFA_H

#include <map>
#include <vector>

#include "DFA.h"

// Rows built in memory for a small computed DFA, by the same rules as
// DedupedDFA: add_state returns the existing state for a repeated row, and
// a row that goes uniformly to reject or accept is that state rather than a
// new one. Built before the TableDFA view is constructed, since a view
// needs its layer sizes up front.
class DFATable
{
  dfa_shape_t shape;
  std::vector<std::vector<dfa_state_t>> rows; // per layer, row after row
  std::vector<std::map<std::vector<dfa_state_t>, dfa_state_t>> lookup;
  dfa_state_t initial_state = ~dfa_state_t(0);

  friend class TableDFA;

 public:

  explicit DFATable(const dfa_shape_t&);

  // A state of layer whose transitions are next_states, each a state of
  // layer + 1 (0 and 1 after the last layer).
  dfa_state_t add_state(int layer, const std::vector<dfa_state_t>& next_states);
  void set_initial_state(dfa_state_t);

  std::vector<size_t> layer_sizes() const;
};

// A view serving a DFATable's rows.
class TableDFA : public DFA
{
  DFATable table;

 protected:

  explicit TableDFA(DFATable&&);

 public:

  virtual dfa_state_t get_transition(int, size_t, int) const;
};

#endif
