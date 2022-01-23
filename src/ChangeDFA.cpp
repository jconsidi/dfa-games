// ChangeDFA.cpp

#include "ChangeDFA.h"

#include <algorithm>
#include <iostream>

template<int ndim, int... shape_pack>
ChangeDFA<ndim, shape_pack...>::ChangeDFA(const typename ChangeDFA<ndim, shape_pack...>::dfa_type& dfa_in, change_func change_rule)
{
  assert(dfa_in.ready());
  this->add_uniform_states();

  std::vector<uint64_t> previous_layer = {0, 1};
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      std::vector<uint64_t> current_layer;

      int layer_shape = this->get_layer_shape(layer);

      std::vector<std::vector<int>> new_values_to_old_values(layer_shape);
      for(int new_value = 0; new_value < layer_shape; ++new_value)
	{
	  for(int old_value = 0; old_value < layer_shape; ++old_value)
	    {
	      if(change_rule(layer, old_value, new_value))
		{
		  new_values_to_old_values[new_value].push_back(old_value);
		}
	    }
	}

      int layer_size = dfa_in.get_layer_size(layer);
      for(int old_index = 0; old_index < layer_size; ++old_index)
	{
	  const DFATransitions& old_transitions = dfa_in.get_transitions(layer, old_index);
	  // TODO: extract changes for this layer
	  // TODO: rewrite layer states based on changes

	  current_layer.push_back(this->add_state(layer, [&](int i)
	  {
	    const std::vector<int>& old_values = new_values_to_old_values[i];

	    std::vector<uint64_t> old_value_states;
	    for(int j = 0; j < old_values.size(); ++j)
	      {
		old_value_states.push_back(previous_layer.at(old_transitions[old_values[j]]));
	      }

	    return union_local(layer, old_value_states);
	  }));
	}

      previous_layer = current_layer;
    }

  assert(this->ready());
}

template<int ndim, int... shape_pack>
uint64_t ChangeDFA<ndim, shape_pack...>::union_local(int layer, std::vector<uint64_t>& states_in)
{
  // sort to normalize

  std::sort(states_in.begin(), states_in.end());

  // dedupe inputs

  for(int i = states_in.size() - 1; i >= 1; --i)
    {
      if(states_in[i] == states_in[i - 1])
	{
	  states_in.erase(states_in.begin() + i);
	}
    }

  // remove reject input (will be first)

  if(states_in.size() > 0)
    {
      if(states_in[0] == 0)
	{
	  states_in.erase(states_in.begin());
	}
    }

  // do stuff

  int num_states = states_in.size();

  // trivial cases based on number of states combined

  if(num_states == 0)
    {
      // no inputs = reject all
      return 0;
    }

  if(num_states == 1)
    {
      // identity since only one distinct state
      return states_in[0];
    }

  // check for accept all state (will be first)

  if(states_in[0] == 1)
    {
      return 0;
    }

  // union multiple states

  std::cout << "union_local(" << states_in[0];
  for(int i = 1; i < states_in.size(); ++i)
    {
      std::cout << ", " << states_in[i];
    }
  std::cout << ")" << std::endl;

  throw std::logic_error("ChangeDFA::union_local not implemented");
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class ChangeDFA<CHESS_DFA_PARAMS>;
template class ChangeDFA<TICTACTOE2_DFA_PARAMS>;
template class ChangeDFA<TICTACTOE3_DFA_PARAMS>;
template class ChangeDFA<TICTACTOE4_DFA_PARAMS>;
