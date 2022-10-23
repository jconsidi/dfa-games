// StringDFA.cpp

#include "StringDFA.h"

template <int ndim, int... shape_pack>
StringDFA<ndim, shape_pack...>::StringDFA(const std::vector<DFAString<ndim, shape_pack...>>& strings_in)
{
  this->set_initial_state(build_internal(0, strings_in));
}

template <int ndim, int... shape_pack>
dfa_state_t StringDFA<ndim, shape_pack...>::build_internal(int layer,
							   const std::vector<DFAString<ndim, shape_pack...>>& strings_in)
{
  if(strings_in.size() <= 0)
    {
      // no more longer following any input strings
      return 0;
    }

  if(layer == ndim)
    {
      // reached the end of a string
      return 1;
    }

  // split input strings by current layer's character

  int layer_shape = this->get_layer_shape(layer);
  std::vector<std::vector<DFAString<ndim, shape_pack...>>> child_strings(layer_shape);

  for(const DFAString<ndim, shape_pack...>& string : strings_in)
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


// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(StringDFA);
