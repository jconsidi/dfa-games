// ChangeDFA.h

#ifndef CHANGE_DFA_H
#define CHANGE_DFA_H

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "RelabelDFA.h"

typedef std::pair<int, int> change_type;
typedef std::optional<change_type> change_optional;
typedef std::vector<change_optional> change_vector;

// Rewrites each changed layer's before character to its after character,
// as a view over the input with the same states.
//
// The input must already be restricted to each changed layer's before
// character, as MoveGraph::add_node guarantees by intersecting
// fixed(layer, before) into every edge entering a changed node. The
// constructor checks this against the input's linear bound and throws
// std::invalid_argument if any changed layer allows another character.
class ChangeDFA
  : public RelabelDFA
{
private:

  static std::vector<std::vector<int>> sources_for(const DFA&, const change_vector&);

public:

  ChangeDFA(shared_dfa_ptr, const change_vector&);
};

#endif
