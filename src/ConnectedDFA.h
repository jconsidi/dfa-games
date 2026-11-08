// ConnectedDFA.h

#ifndef CONNECTED_DFA_H
#define CONNECTED_DFA_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "DedupedDFA.h"

typedef uint32_t connected_mask_t;

// Counts groups: maximal sets of adjacent cells of the same class, where each
// layer is one cell.
struct ConnectedSpec
{
  // Adjacency between cells, as pairs of layers. Symmetric; no self loops or
  // duplicates.
  std::vector<std::pair<int, int>> edges;

  // class_of[layer][c] is the class of character c at that layer, in
  // [0, 32), or -1 if the character has no class.
  std::vector<std::vector<int>> class_of;

  // Static labels carried by each layer's cell, bit i for label i.
  std::vector<connected_mask_t> labels;

  // Which groups are counted: those of a class in group_classes that, once
  // complete,
  //   - touch every label in labels_required and none in labels_forbidden,
  //   - border a cell of every class in adjacent_required and of none in
  //     adjacent_forbidden.
  connected_mask_t group_classes = 0;
  connected_mask_t labels_required = 0;
  connected_mask_t labels_forbidden = 0;
  connected_mask_t adjacent_required = 0;
  connected_mask_t adjacent_forbidden = 0;

  // Accept iff the number of counted groups is in [count_min, count_max].
  // count_max < 0 means unbounded.
  int count_min = 1;
  int count_max = -1;

  // sha256 of a canonical serialization of the shape and spec, for naming
  // cached DFAs. The shape is hashed rather than spelled out in the name, so
  // the name stays short on any board.
  std::string cache_key(const dfa_shape_t&) const;

  // Throws std::invalid_argument if the spec does not fit the shape.
  void validate(const dfa_shape_t&) const;
};

// Built by the frontier method: the state after each layer records, for the
// cells already read that still have an unread neighbour, their class and
// group, plus what each open group has collected and how many closed groups
// were counted.
class ConnectedDFA : public DedupedDFA
{
 public:

  ConnectedDFA(const dfa_shape_t&, const ConnectedSpec&);
};

#endif
