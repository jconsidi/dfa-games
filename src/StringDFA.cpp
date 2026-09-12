// StringDFA.cpp

#include "StringDFA.h"

StringDFA::StringDFA(const dfa_shape_t& shape_in, const std::vector<DFAString>& strings_in)
  : DedupedDFA(shape_in)
{
  std::vector<std::reference_wrapper<const DFAString>> strings(strings_in.begin(), strings_in.end());

  // Must be decided before set_initial_state, not after -- see
  // DFA::set_canonical.
  if(strings_in.size() <= 1)
    {
      set_canonical(1);
    }

  this->set_initial_state(build_internal(0, strings));
}

dfa_state_t StringDFA::build_internal(int layer,
				      const std::vector<std::reference_wrapper<const DFAString>>& strings_in)
{
  if(strings_in.size() <= 0)
    {
      // no more longer following any input strings
      return 0;
    }

  if(layer == get_shape_size())
    {
      // reached the end of a string
      return 1;
    }

  // split input strings by current layer's character

  int layer_shape = this->get_layer_shape(layer);
  std::vector<std::vector<std::reference_wrapper<const DFAString>>> child_strings(layer_shape);

  for(const DFAString& string : strings_in)
    {
      child_strings.at(string[layer]).push_back(string);
    }

  DFATransitionsStaging transitions;
  for(int i = 0; i < layer_shape; ++i)
    {
      transitions.push_back(build_internal(layer + 1, child_strings.at(i)));
    }

  return this->add_state(layer, transitions);
}
