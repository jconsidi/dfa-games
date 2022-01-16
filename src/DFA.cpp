// DFA.cpp

#include <assert.h>

#include <iostream>

#include "DFA.h"

template<int... shape_pack>
std::vector<int> shape_pack_to_vector()
{
  // initialize an array from the shape parameter pack

  int ox[] = {shape_pack...};

  // convert to a vector and return
  std::vector<int> output;
  for(int i = 0; i < sizeof(ox) / sizeof(int); ++i)
    {
      output.push_back(ox[i]);
    }
  output.shrink_to_fit();

  return output;
}

template<int ndim, int... shape_pack>
DFA<ndim, shape_pack...>::DFA()
  : shape(shape_pack_to_vector<shape_pack...>()),
    state_transitions(new std::vector<DFATransitions>[ndim]),
    state_lookup(new DFATransitionsMap[ndim])
{
  assert(shape.size() == ndim);
}

template<int ndim, int... shape_pack>
DFA<ndim, shape_pack...>::~DFA()
{
  if(this->state_lookup)
    {
      delete[] this->state_lookup;
      this->state_lookup = 0;
    }

  delete[] this->state_transitions;
}

template<int ndim, int... shape_pack>
int DFA<ndim, shape_pack...>::add_state(int layer, const DFATransitions& next_states)
{
  assert(state_lookup);
  assert((0 <= layer) && (layer < ndim));
  assert(next_states.size() == this->shape[layer]);

  if(layer == 0)
    {
      // only one initial state allowed
      if(state_transitions[0].size() > 0)
	{
	  throw std::logic_error("tried to add second initial state");
	}

      delete[] state_lookup;
      state_lookup = 0;
    }

  // state range checks

  int layer_shape = this->shape[layer];

  if(layer < ndim - 1)
    {
      // make sure next states exist
      int next_layer_size = state_transitions[layer + 1].size();
      for(int i = 0; i < layer_shape; ++i)
	{
	  assert(next_states[i] < next_layer_size);
	}
    }
  else
    {
      // last layer is all zero/one for reject/accept.
      for(int i = 0; i < layer_shape; ++i)
	{
	  assert(next_states[i] < 2);
	}
    }

  if(layer > 0)
    {
      if(this->state_lookup[layer].count(next_states))
	{
	  return this->state_lookup[layer][next_states];
	}
    }

  // add new state

  int new_state_id = this->state_transitions[layer].size();
  this->state_transitions[layer].emplace_back(next_states);

  assert(this->state_transitions[layer].size() == (new_state_id + 1));

  if(layer > 0)
    {
      this->state_lookup[layer][next_states] = new_state_id;
    }

  return new_state_id;
}

template<int ndim, int... shape_pack>
int DFA<ndim, shape_pack...>::add_state(int layer, std::function<uint64_t(int)> transition_func)
{
  int layer_shape = shape[layer];

  DFATransitions transitions(layer_shape);
  for(int i = 0; i < layer_shape; ++i)
    {
      transitions[i] = transition_func(i);
    }

  return add_state(layer, transitions);
}

template<int ndim, int... shape_pack>
void DFA<ndim, shape_pack...>::add_uniform_states()
{
  for(int layer = ndim - 1; layer >= 1; --layer)
    {
      uint64_t reject_state = add_state(layer, [](int i){return 0;});
      assert(reject_state == 0);

      uint64_t accept_state = add_state(layer, [](int i){return 1;});
      assert(accept_state == 1);
    }
}

template<int ndim, int... shape_pack>
int DFA<ndim, shape_pack...>::get_layer_shape(int layer) const
{
  assert((0 <= layer) && (layer < ndim));
  return shape[layer];
}

template<int ndim, int... shape_pack>
int DFA<ndim, shape_pack...>::get_layer_size(int layer) const
{
  return state_transitions[layer].size();
}

template<int ndim, int... shape_pack>
const DFATransitions& DFA<ndim, shape_pack...>::get_transitions(int layer, int state_index) const
{
  return state_transitions[layer].at(state_index);
}

template<int ndim, int... shape_pack>
bool DFA<ndim, shape_pack...>::ready() const
{
  return state_transitions[0].size() != 0;
}

template<int ndim, int... shape_pack>
double DFA<ndim, shape_pack...>::size() const
{
  assert(state_transitions[0].size() == 1);

  std::vector<double> previous_counts({0, 1}); // reject, accept
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);

      std::vector<double> current_counts;
      for(int state_index = 0; state_index < state_transitions[layer].size(); ++state_index)
	{
	  const DFATransitions& transitions = this->get_transitions(layer, state_index);

	  double state_count = 0;
	  for(int i = 0; i < layer_shape; ++i)
	    {
	      state_count += previous_counts.at(transitions[i]);
	    }
	  current_counts.push_back(state_count);
	}

      previous_counts = current_counts;
    }

  assert(previous_counts.size() == 1);

  return previous_counts[0];
}

template<int ndim, int... shape_pack>
int DFA<ndim, shape_pack...>::states() const
{
  int states_out = 0;

  for(int layer = 0; layer < ndim; ++layer)
    {
      assert(state_transitions[layer].size() > 0);
      states_out += state_transitions[layer].size();
    }

  return states_out;
}

// template instantiations

#include "ChessDFA.h"
#include "TicTacToeGame.h"

template class DFA<CHESS_TEMPLATE_PARAMS>;
template class DFA<TEST_DFA_PARAMS>;
template class DFA<TICTACTOE2_DFA_PARAMS>;
template class DFA<TICTACTOE3_DFA_PARAMS>;
template class DFA<TICTACTOE4_DFA_PARAMS>;
