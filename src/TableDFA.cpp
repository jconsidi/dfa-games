// TableDFA.cpp

#include "TableDFA.h"

#include <format>
#include <stdexcept>

DFATable::DFATable(const dfa_shape_t& shape_in)
  : shape(shape_in),
    rows(shape_in.size()),
    lookup(shape_in.size())
{
}

dfa_state_t DFATable::add_state(int layer, const std::vector<dfa_state_t>& next_states)
{
  if((layer < 0) || (layer >= int(shape.size())) || (next_states.size() != size_t(shape[layer])))
    {
      throw std::logic_error(std::format("DFATable::add_state: bad layer {:d} or row of {:d} entries", layer, next_states.size()));
    }

  auto search = lookup[layer].find(next_states);
  if(search != lookup[layer].end())
    {
      return search->second;
    }

  dfa_state_t output;
  bool uniform = true;
  for(dfa_state_t next_state : next_states)
    {
      uniform = uniform && (next_state == next_states[0]);
    }
  if(uniform && (next_states[0] < 2))
    {
      output = next_states[0];
    }
  else
    {
      size_t next_layer_size = (layer + 1 < int(shape.size())) ? (2 + rows[layer + 1].size() / size_t(shape[layer + 1])) : 2;
      for(dfa_state_t next_state : next_states)
	{
	  if(next_state >= next_layer_size)
	    {
	      throw std::logic_error(std::format("DFATable::add_state: layer {:d} transition to {:d}, outside the next layer's {:d} states", layer, next_state, next_layer_size));
	    }
	}
      output = dfa_state_t(2 + rows[layer].size() / size_t(shape[layer]));
      rows[layer].insert(rows[layer].end(), next_states.begin(), next_states.end());
    }

  lookup[layer][next_states] = output;
  return output;
}

void DFATable::set_initial_state(dfa_state_t initial_state_in)
{
  initial_state = initial_state_in;
  lookup.clear();
}

std::vector<size_t> DFATable::layer_sizes() const
{
  std::vector<size_t> output;
  for(size_t layer = 0; layer < shape.size(); ++layer)
    {
      output.push_back(2 + rows[layer].size() / size_t(shape[layer]));
    }
  return output;
}

TableDFA::TableDFA(DFATable&& table_in)
  : DFA(table_in.shape, table_in.layer_sizes(), table_in.initial_state),
    table(std::move(table_in))
{
}

dfa_state_t TableDFA::get_transition(int layer, size_t state, int c) const
{
  return table.rows[layer][(state - 2) * size_t(get_layer_shape(layer)) + size_t(c)];
}
