// StringDFA.cpp

#include "StringDFA.h"

StringDFA::StringDFA(const std::vector<DFAString>& strings_in)
  : DedupedDFA(strings_in[0].get_shape())
{
  this->set_initial_state(build_internal(0, strings_in));
}

dfa_state_t StringDFA::build_internal(int layer,
				      const std::vector<DFAString>& strings_in)
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
  std::vector<std::vector<DFAString>> child_strings(layer_shape);

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
