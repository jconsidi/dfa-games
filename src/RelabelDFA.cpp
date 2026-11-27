// RelabelDFA.cpp

#include "RelabelDFA.h"

#include <format>
#include <stdexcept>

std::vector<size_t> RelabelDFA::view_layer_sizes(const DFA& base_in)
{
  std::vector<size_t> output;
  for(int layer = 0; layer < base_in.get_shape_size(); ++layer)
    {
      // a constant base's ordinary states are unreachable, so the view
      // keeps only the reserved rows
      output.push_back((base_in.get_initial_state() < 2) ? 2 : base_in.get_layer_size(layer));
    }
  return output;
}

RelabelDFA::RelabelDFA(shared_dfa_ptr base_in, const std::vector<std::vector<int>>& sources_in)
  : DFA(base_in->get_shape(), view_layer_sizes(*base_in), base_in->get_initial_state()),
    base(base_in),
    sources(sources_in)
{
  if(sources.size() != size_t(get_shape_size()))
    {
      throw std::invalid_argument(std::format("RelabelDFA: {:d} layers of sources for {:d} layers", sources.size(), get_shape_size()));
    }

  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      const std::vector<int>& layer_sources = sources[layer];
      if(layer_sources.empty())
	{
	  continue;
	}

      int layer_shape = get_layer_shape(layer);
      if(layer_sources.size() != size_t(layer_shape))
	{
	  throw std::invalid_argument(std::format("RelabelDFA: layer {:d} has {:d} sources for {:d} characters", layer, layer_sources.size(), layer_shape));
	}

      std::vector<bool> seen(layer_shape, false);
      for(int source : layer_sources)
	{
	  if(source < 0)
	    {
	      if(source != -1)
		{
		  throw std::invalid_argument(std::format("RelabelDFA: layer {:d} has source {:d}", layer, source));
		}
	      continue;
	    }
	  if((source >= layer_shape) || seen[source])
	    {
	      throw std::invalid_argument(std::format("RelabelDFA: layer {:d} source {:d} is outside the alphabet or repeated", layer, source));
	    }
	  seen[source] = true;
	}
    }
}

dfa_state_t RelabelDFA::get_transition(int layer, size_t state, int c) const
{
  const std::vector<int>& layer_sources = sources[layer];
  if(layer_sources.empty())
    {
      return base->get_transitions(layer, state)[c];
    }

  int source = layer_sources[c];
  if(source < 0)
    {
      return 0;
    }
  return base->get_transitions(layer, state)[source];
}

void RelabelDFA::mmap() const
{
  base->mmap();
}

void RelabelDFA::munmap() const
{
  // other views may share the base, so it stays mapped while this view
  // holds it
}

double RelabelDFA::size() const
{
  return base->size();
}
