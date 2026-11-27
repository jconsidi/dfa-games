// InverseDFA.cpp

#include "InverseDFA.h"

std::vector<size_t> InverseDFA::view_layer_sizes(const DFA& dfa_in)
{
  std::vector<size_t> output;
  for(int layer = 0; layer < dfa_in.get_shape_size(); ++layer)
    {
      // a constant input's ordinary states are unreachable, so its inverse
      // keeps only the reserved rows
      output.push_back((dfa_in.get_initial_state() < 2) ? 2 : dfa_in.get_layer_size(layer));
    }
  return output;
}

dfa_state_t InverseDFA::view_initial_state(const DFA& dfa_in)
{
  dfa_state_t initial = dfa_in.get_initial_state();
  return (initial < 2) ? (1 - initial) : initial;
}

InverseDFA::InverseDFA(shared_dfa_ptr input_in)
  : DFA(input_in->get_shape(), view_layer_sizes(*input_in), view_initial_state(*input_in)),
    input(input_in)
{
}

dfa_state_t InverseDFA::get_transition(int layer, size_t state, int c) const
{
  dfa_state_t next_state = input->get_transitions(layer, state)[c];
  return (next_state < 2) ? (1 - next_state) : next_state;
}

void InverseDFA::mmap() const
{
  input->mmap();
}

void InverseDFA::munmap() const
{
  // other views may share the input, so it stays mapped while this view
  // holds it
}

double InverseDFA::size() const
{
  double total = 1.0;
  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      total *= double(get_layer_shape(layer));
    }
  return total - input->size();
}
