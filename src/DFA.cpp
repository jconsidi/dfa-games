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
    state_transitions(new std::vector<DFATransitions>[ndim])
{
  assert(shape.size() == ndim);
}

template<int ndim, int... shape_pack>
DFA<ndim, shape_pack...>::~DFA()
{
  delete[] this->state_transitions;
}

template<int ndim, int... shape_pack>
void DFA<ndim, shape_pack...>::set_initial_state(dfa_state_t initial_state_in)
{
  assert(initial_state == ~dfa_state_t(0));

  assert(initial_state_in < state_transitions[0].size());
  initial_state = initial_state_in;
}

template<int ndim, int... shape_pack>
DFAIterator<ndim, shape_pack...> DFA<ndim, shape_pack...>::cbegin() const
{
  if(initial_state == 0)
    {
      return cend();
    }

  std::vector<int> characters;

  dfa_state_t current_state = initial_state;
  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = get_layer_shape(layer);
      const DFATransitions& transitions = get_transitions(layer, current_state);

      // scan for first accepted character
      for(characters.push_back(0);
	  (characters[layer] < layer_shape) && !transitions[characters[layer]];
	  ++characters[layer])
	{
	}
      assert(characters[layer] < layer_shape);
      assert(transitions[characters[layer]]);

      current_state = transitions[characters[layer]];
    }

  assert(current_state == 1);

  return DFAIterator<ndim, shape_pack...>(*this, characters);
}

template<int ndim, int... shape_pack>
DFAIterator<ndim, shape_pack...> DFA<ndim, shape_pack...>::cend() const
{
  std::vector<int> characters;
  characters.push_back(shape[0]);
  for(int layer = 1; layer < ndim; ++layer)
    {
      characters.push_back(0);
    }

  return DFAIterator<ndim, shape_pack...>(*this, characters);
}

template<int ndim, int... shape_pack>
void DFA<ndim, shape_pack...>::debug_example(std::ostream& os) const
{
  // identify reject state (if any) at each layer

  std::vector<int> reject_states(ndim+1);
  reject_states[ndim] = 0;
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      reject_states[layer] = -1;

      int layer_shape = this->get_layer_shape(layer);
      int layer_size = this->get_layer_size(layer);
      for(int state_index = 0; state_index < layer_size; ++state_index)
	{
	  bool maybe_reject = true;
	  const DFATransitions& transitions = this->get_transitions(layer, state_index);
	  for(int i = 0; i < layer_shape; ++i)
	    {
	      if(transitions[i] != reject_states[layer+1])
		{
		  maybe_reject = false;
		  break;
		}
	    }
	  if(maybe_reject)
	    {
	      reject_states[layer] = state_index;
	      break;
	    }
	}
    }

  if(reject_states[0] == 0)
    {
      os << "DFA rejects all inputs" << std::endl;
      return;
    }

  // output first accepted state

  dfa_state_t state_index = 0;
  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = this->get_layer_shape(layer);
      const DFATransitions& transitions = this->get_transitions(layer, state_index);

      for(int i = 0; i < layer_shape; ++i)
	{
	  if(transitions[i] != reject_states[layer+1])
	    {
	      os << "layer[" << layer << "] = " << i << std::endl;
	      state_index = transitions[i];
	      break;
	    }
	}
    }
}

template<int ndim, int... shape_pack>
dfa_state_t DFA<ndim, shape_pack...>::get_initial_state() const
{
  assert(initial_state != ~dfa_state_t(0));
  return initial_state;
}

template<int ndim, int... shape_pack>
int DFA<ndim, shape_pack...>::get_layer_shape(int layer) const
{
  assert((0 <= layer) && (layer < ndim));

  return shape.at(layer);
}

template<int ndim, int... shape_pack>
dfa_state_t DFA<ndim, shape_pack...>::get_layer_size(int layer) const
{
  assert(layer <= ndim);

  if(layer == ndim)
    {
      return 2;
    }

  return state_transitions[layer].size();
}

template<int ndim, int... shape_pack>
const DFATransitions& DFA<ndim, shape_pack...>::get_transitions(int layer, dfa_state_t state_index) const
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
  assert(initial_state != ~dfa_state_t(0));

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

  return previous_counts.at(initial_state);
}

template<int ndim, int... shape_pack>
size_t DFA<ndim, shape_pack...>::states() const
{
  int states_out = 0;

  for(int layer = 0; layer < ndim; ++layer)
    {
      assert(state_transitions[layer].size() > 0);
      states_out += state_transitions[layer].size();
    }

  return states_out;
}

template<int ndim, int... shape_pack>
DFAIterator<ndim, shape_pack...>::DFAIterator(const DFA<ndim, shape_pack...>& dfa_in, const std::vector<int>& characters_in)
  : shape(shape_pack_to_vector<shape_pack...>()),
    dfa(dfa_in),
    characters(characters_in)
{
  assert(characters.size() == ndim);

  if(characters[0] < shape[0])
    {
      // not at end
      for(int i = 1; i < ndim; ++i)
	{
	  assert(characters[i] < shape[i]);
	}
    }
  else
    {
      // unique end
      assert(characters[0] == shape[0]);
      for(int i = 1; i < ndim; ++i)
	{
	  assert(characters[i] == 0);
	}
    }
}

template<int ndim, int... shape_pack>
DFAString<ndim, shape_pack...> DFAIterator<ndim, shape_pack...>::operator*() const
{
  assert(characters[0] < shape[0]);

  return DFAString<ndim, shape_pack...>(characters);
}

template<int ndim, int... shape_pack>
DFAIterator<ndim, shape_pack...>& DFAIterator<ndim, shape_pack...>::operator++()
{
  assert(characters[0] < shape[0]);

  std::vector<dfa_state_t> states;
  states.push_back(dfa.get_initial_state());
  for(int layer = 0; layer < ndim; ++layer)
    {
      assert(characters.at(layer) < shape[layer]);
      states.push_back(dfa.get_transitions(layer, states[layer]).at(characters[layer]));
      assert(states.at(layer + 1) < dfa.get_layer_size(layer + 1));
    }
  assert(states.size() == ndim + 1);
  assert(states[ndim] == 1);

  // advancing is like incrementing a number with carrying, except we
  // also have to skip over non-accepting states.

  states.pop_back();
  assert(states.size() == characters.size());
  while(states.size())
    {
      assert(states.size() == characters.size());

      int layer = states.size() - 1;
      int layer_shape = dfa.get_layer_shape(layer);

      const DFATransitions& transitions = dfa.get_transitions(layer, states[layer]);

      // scan for the next accepting character choice
      assert(characters[layer] < layer_shape);
      for(++characters[layer]; // initial advancement
	  ((characters[layer] < layer_shape) &&
	   !transitions[characters[layer]]);
	  ++characters[layer])
	{
	}
      if(characters[layer] < layer_shape)
	{
	  // found an accepting character/state
	  assert(states.size() == characters.size());
	  states.push_back(transitions[characters[layer]]);
	  assert(states.size() == characters.size() + 1);
	  break;
	}

      // no more character choices work at this layer
      characters.pop_back();
      states.pop_back();
    }

  if(states.size() == 0)
    {
      // no more accepting strings
      characters.push_back(shape[0]);
      for(int layer = 1; layer < ndim; ++layer)
	{
	  characters.push_back(0);
	}

      return *this;
    }

  // fill forward from accepting state found

  assert(states.size() == characters.size() + 1);

  for(int layer = characters.size(); layer < ndim; ++layer)
    {
      // figure out first matching character for next layer
      int layer_shape = dfa.get_layer_shape(layer);
      const DFATransitions& transitions = dfa.get_transitions(layer, states[layer]);

      for(characters.push_back(0);
	  ((characters[layer] < layer_shape) &&
	   (transitions[characters[layer]] == 0));
	  ++characters[layer])
	{
	}

      assert(characters.at(layer) < layer_shape);
      assert(transitions[characters[layer]]);

      states.push_back(transitions[characters[layer]]);
    }
  assert(states.size() == ndim + 1);
  assert(states[ndim] == 1);
  assert(characters.size() == ndim);

  // done

  return *this;
}

template<int ndim, int... shape_pack>
bool DFAIterator<ndim, shape_pack...>::operator<(const DFAIterator<ndim, shape_pack...>& right_in) const
{
  for(int i = 0; i < ndim; ++i)
    {
      int l = characters[i];
      int r = right_in.characters[i];
      if(l < r)
	{
	  return true;
	}
      else if(l > r)
	{
	  return false;
	}
    }

  return false;
}

template<int ndim, int... shape_pack>
DFAString<ndim, shape_pack...>::DFAString(const std::vector<int>& characters_in)
  : characters(characters_in)
{
  assert(characters.size() == ndim);

  auto shape_temp = shape_pack_to_vector<shape_pack...>();
  for(int i = 0; i < ndim; ++i)
    {
      assert(characters.at(i) < shape_temp.at(i));
    }
}

template<int ndim, int... shape_pack>
int DFAString<ndim, shape_pack...>::operator[](int layer_in) const
{
  return characters.at(layer_in);
}

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(DFA);
INSTANTIATE_DFA_TEMPLATE(DFAIterator);
INSTANTIATE_DFA_TEMPLATE(DFAString);
