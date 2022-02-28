// ChangeDFA.cpp

#include "ChangeDFA.h"

#include <algorithm>
#include <iostream>
#include <sstream>

template<int ndim, int... shape_pack>
ChangeDFA<ndim, shape_pack...>::ChangeDFA(const typename ChangeDFA<ndim, shape_pack...>::dfa_type& dfa_in, change_func change_rule)
  : new_values_to_old_values_by_layer(),
    change_cache(ndim),
    union_local_cache(ndim)
{
  assert(dfa_in.ready());
  this->dfa_temp = &dfa_in;

  this->add_uniform_states();

  // setup new_values_to_old_values_by_layer

  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = this->get_layer_shape(layer);

      new_values_to_old_values_by_layer.emplace_back(layer_shape);
      std::vector<std::vector<int>>& new_values_to_old_values = new_values_to_old_values_by_layer[layer];
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
    }

  // pre-cache rejects

  for(int layer = 1; layer < ndim; ++layer)
    {
      change_cache[layer][0] = 0;
    }

  // trigger whole change process

  this->change_state(0, 0);

  assert(this->ready());

  // cleanup temporary state

  this->dfa_temp = 0;

  this->union_local_cache.clear();
  this->union_local_cache.shrink_to_fit();

  this->change_cache.clear();
  this->change_cache.shrink_to_fit();

  this->new_values_to_old_values_by_layer.clear();
  this->new_values_to_old_values_by_layer.shrink_to_fit();
}

template<int ndim, int... shape_pack>
dfa_state_t ChangeDFA<ndim, shape_pack...>::change_state(int layer, dfa_state_t state_index)
{
  if(layer == ndim)
    {
      return state_index;
    }

  auto search = this->change_cache[layer].find(state_index);
  if(search != this->change_cache[layer].end())
    {
      return search->second;
    }

  const DFATransitions& old_transitions = dfa_temp->get_transitions(layer, state_index);

  dfa_state_t new_index = this->add_state(layer, [=](int new_value)
  {
    const std::vector<int>& old_values = new_values_to_old_values_by_layer[layer][new_value];

    std::vector<dfa_state_t> states_to_union;
    for(int i = 0; i < old_values.size(); ++i)
      {
	states_to_union.push_back(change_state(layer+1, old_transitions[old_values[i]]));
      }

    return union_local(layer+1, states_to_union);
  });

  this->change_cache[layer][state_index] = new_index;
  return new_index;
}

template<int ndim, int... shape_pack>
dfa_state_t ChangeDFA<ndim, shape_pack...>::union_local(int layer, std::vector<dfa_state_t>& states_in)
{
  assert((1 <= layer) && (layer <= ndim));

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

  // trivial cases based on number of remaining states

  int num_states = states_in.size();
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

  // sanity checks

  assert(layer < ndim);
  assert(states_in[num_states - 1] < this->get_layer_size(layer));

  // union multiple states

  std::ostringstream key_builder;
  key_builder << states_in[0];
  for(int i = 1; i < num_states; ++i)
    {
      key_builder << "/" << states_in[i];
    }
  std::string key = key_builder.str();

  auto search = this->union_local_cache[layer].find(key);
  if(search != this->union_local_cache[layer].end())
    {
      return search->second;
    }

  auto output = this->add_state(layer, [&](int j)
  {
    std::vector<dfa_state_t> states_j;
    for(int i = 0; i < num_states; ++i)
      {
	states_j.push_back(this->get_transitions(layer, states_in[i])[j]);
      }
    return union_local(layer+1, states_j);
  });

  this->union_local_cache[layer][key] = output;
  return output;
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class ChangeDFA<CHESS_DFA_PARAMS>;
template class ChangeDFA<TICTACTOE2_DFA_PARAMS>;
template class ChangeDFA<TICTACTOE3_DFA_PARAMS>;
template class ChangeDFA<TICTACTOE4_DFA_PARAMS>;
