// DFA.cpp

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "DFA.h"

static int next_dfa_id = 0;

std::vector<std::string> get_layer_file_names(int ndim, std::string directory)
{
  std::vector<std::string> output;

  for(int layer = 0; layer < ndim; ++layer)
    {
      output.push_back(directory + "/layer=" + std::to_string(layer));
    }

  return output;
}

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

DFATransitionsReference::DFATransitionsReference(const MemoryMap<dfa_state_t>& layer_transitions_in,
						 dfa_state_t state_in,
						 int layer_shape_in)
  : layer_transitions(layer_transitions_in),
    offset(size_t(state_in) * size_t(layer_shape_in)),
    layer_shape(layer_shape_in)
{
  assert(layer_shape > 0);
  size_t size_temp = layer_transitions.size();
  assert(offset < size_temp);
  assert(offset + layer_shape <= size_temp);
}

template<int ndim, int... shape_pack>
DFA<ndim, shape_pack...>::DFA()
  : shape(shape_pack_to_vector<shape_pack...>()),
    directory("/tmp/chess/dfa-" + std::to_string(next_dfa_id++)),
    layer_file_names(get_layer_file_names(ndim, directory)),
    layer_sizes(),
    layer_transitions(),
    temporary(true)
{
  assert(shape.size() == ndim);

  mkdir(directory.c_str(), 0700);

  for(int layer = 0; layer < ndim; ++layer)
    {
      layer_sizes.push_back(0);
      layer_transitions.emplace_back(layer_file_names.at(layer), 1024);
    }

  assert(layer_sizes.size() == ndim);
  assert(layer_file_names.size() == ndim);
  assert(layer_transitions.size() == ndim);
}

template<int ndim, int... shape_pack>
DFA<ndim, shape_pack...>::~DFA() noexcept(false)
{
  if(temporary)
    {
      for(int layer = 0; layer < ndim; ++layer)
	{
	  int ret = unlink(layer_file_names.at(layer).c_str());
	  if(ret)
	    {
	      perror("DFA file cleanup");
	      throw std::runtime_error("DFA file cleanup failed");
	    }
	}

      int ret = rmdir(directory.c_str());
      if(ret)
	{
	  perror("DFA directory cleanup");
	  throw std::runtime_error("DFA directory cleanup failed");
	}
    }
}

template<int ndim, int... shape_pack>
void DFA<ndim, shape_pack...>::set_initial_state(dfa_state_t initial_state_in)
{
  assert(initial_state == ~dfa_state_t(0));

  assert(initial_state_in < get_layer_size(0));
  initial_state = initial_state_in;

  finalize();
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
      DFATransitionsReference transitions = get_transitions(layer, current_state);

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
	  DFATransitionsReference transitions = this->get_transitions(layer, state_index);
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
      DFATransitionsReference transitions = this->get_transitions(layer, state_index);

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
void DFA<ndim, shape_pack...>::emplace_transitions(int layer, const DFATransitionsStaging& transitions)
{
  assert(layer < ndim);

  int layer_shape = get_layer_shape(layer);
  assert(transitions.size() == layer_shape);

  size_t current_offset = size_t(layer_sizes[layer]) * size_t(layer_shape);
  size_t next_offset = current_offset + size_t(layer_shape);

  MemoryMap<dfa_state_t>& current_transitions = layer_transitions[layer];
  size_t current_size = current_transitions.size();
  if(next_offset > current_size)
    {
      size_t next_size = current_size * 2;
      current_transitions = MemoryMap<dfa_state_t>(layer_file_names[layer], next_size);
    }

  for(int i = 0; i < layer_shape; ++i)
    {
      current_transitions[current_offset + i] = transitions[i];
    }

  ++layer_sizes[layer];
}

template<int ndim, int... shape_pack>
void DFA<ndim, shape_pack...>::finalize()
{
  assert(ready());

  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = get_layer_shape(layer);
      size_t expected_transitions_size = size_t(layer_sizes[layer]) * size_t(layer_shape);
      if(layer_transitions[layer].size() != expected_transitions_size)
	{
	  layer_transitions[layer] = MemoryMap<dfa_state_t>(layer_file_names[layer], expected_transitions_size);
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

  return layer_sizes[layer];
}

template<int ndim, int... shape_pack>
DFATransitionsReference DFA<ndim, shape_pack...>::get_transitions(int layer, dfa_state_t state_index) const
{
  assert(layer < ndim);
  assert(state_index < layer_sizes[layer]);
  return DFATransitionsReference(layer_transitions[layer], state_index, get_layer_shape(layer));
}

template<int ndim, int... shape_pack>
bool DFA<ndim, shape_pack...>::ready() const
{
  return initial_state != ~dfa_state_t(0);
}

template<int ndim, int... shape_pack>
double DFA<ndim, shape_pack...>::size() const
{
  assert(ready());

  std::vector<double> previous_counts({0, 1}); // reject, accept
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);

      std::vector<double> current_counts;
      dfa_state_t layer_size = get_layer_size(layer);
      for(dfa_state_t state_index = 0; state_index < layer_size; ++state_index)
	{
	  DFATransitionsReference transitions = this->get_transitions(layer, state_index);

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
  size_t states_out = 0;

  for(int layer = 0; layer < ndim; ++layer)
    {
      dfa_state_t layer_size = get_layer_size(layer);
      assert(layer_size > 0);
      states_out += layer_size;
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

      DFATransitionsReference transitions = dfa.get_transitions(layer, states[layer]);

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
      DFATransitionsReference transitions = dfa.get_transitions(layer, states[layer]);

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
