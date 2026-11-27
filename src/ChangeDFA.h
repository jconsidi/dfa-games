// ChangeDFA.h

#ifndef CHANGE_DFA_H
#define CHANGE_DFA_H

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "DFA.h"

typedef std::pair<int, int> change_type;
typedef std::optional<change_type> change_optional;
typedef std::vector<change_optional> change_vector;

// Rewrites each changed layer's before character to its after character.
//
// The input must already be restricted to each changed layer's before
// character, as MoveGraph::add_node guarantees by intersecting
// fixed(layer, before) into every edge entering a changed node. The
// constructor checks this against the input's linear bound and throws
// std::invalid_argument if any changed layer allows another character.
// The output keeps every input state and its number.
class ChangeDFA
  : public DFA
{
private:

  void build_one_pass(const DFA&, const change_vector&);

public:

  ChangeDFA(const DFA&, const change_vector&);
};

#endif
