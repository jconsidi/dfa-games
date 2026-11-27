// ChangeDFA.cpp

#include "ChangeDFA.h"

#include <format>
#include <stdexcept>
#include <string>

std::vector<std::vector<int>> ChangeDFA::sources_for(const DFA& dfa_in, const change_vector& changes_in)
{
  if(changes_in.size() != size_t(dfa_in.get_shape_size()))
    {
      throw std::invalid_argument(std::format("ChangeDFA: {:d} changes for {:d} layers", changes_in.size(), dfa_in.get_shape_size()));
    }

  dfa_in.mmap();

  const DFALinearBound& linear_bound_in = dfa_in.get_linear_bound();
  std::vector<std::vector<int>> sources(changes_in.size());
  for(int layer = 0; layer < dfa_in.get_shape_size(); ++layer)
    {
      change_optional layer_change = changes_in[layer];
      if(!layer_change.has_value())
	{
	  continue;
	}

      int before_character = std::get<0>(*layer_change);
      int after_character = std::get<1>(*layer_change);
      int layer_shape = dfa_in.get_layer_shape(layer);
      if((before_character < 0) || (before_character >= layer_shape) || (after_character < 0) || (after_character >= layer_shape))
	{
	  throw std::invalid_argument(std::format("ChangeDFA: change {:d} -> {:d} at layer {:d} is outside its alphabet of {:d}",
						  before_character, after_character, layer, layer_shape));
	}
      if(!linear_bound_in.check_fixed(layer, before_character))
	{
	  std::string allowed;
	  for(int c = 0; c < layer_shape; ++c)
	    {
	      if(linear_bound_in.check_bound(layer, c))
		{
		  allowed += (allowed.empty() ? "" : ",") + std::to_string(c);
		}
	    }
	  throw std::invalid_argument(std::format("ChangeDFA: input is not restricted to before character {:d} at layer {:d}; its linear bound allows {{{:s}}}",
						  before_character, layer, allowed));
	}

      // the after character takes the before character's transition, and
      // every other character rejects
      sources[layer] = std::vector<int>(layer_shape, -1);
      sources[layer][after_character] = before_character;
    }

  return sources;
}

ChangeDFA::ChangeDFA(shared_dfa_ptr dfa_in, const change_vector& changes_in)
  : RelabelDFA(dfa_in, sources_for(*dfa_in, changes_in))
{
}
