// ChangeDFA.cpp

#include "ChangeDFA.h"

#include <format>
#include <stdexcept>
#include <string>

#include "Profile.h"

ChangeDFA::ChangeDFA(const DFA& dfa_in,
		     const change_vector& changes_in)
  : DFA(dfa_in.get_shape())
{
  if(changes_in.size() != size_t(dfa_in.get_shape_size()))
    {
      throw std::invalid_argument(std::format("ChangeDFA: {:d} changes for {:d} layers", changes_in.size(), dfa_in.get_shape_size()));
    }

  dfa_in.mmap();

  const DFALinearBound& linear_bound_in = dfa_in.get_linear_bound();
  for(int layer = 0; layer < dfa_in.get_shape_size(); ++layer)
    {
      change_optional layer_change = changes_in[layer];
      if(layer_change.has_value())
	{
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
	}
    }

  build_one_pass(dfa_in, changes_in);
}

void ChangeDFA::build_one_pass(const DFA& dfa_in, const change_vector& changes_in)
{
  Profile profile("build_one_pass");

  // rewrites DFA in one pass. the constructor has checked that the
  // input is restricted to each changed layer's before character.

  const DFALinearBound& linear_bound_in = dfa_in.get_linear_bound();

  for(int layer = get_shape_size() - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);
      size_t layer_size = dfa_in.get_layer_size(layer);

      change_optional layer_change = changes_in[layer];

      if(layer_change.has_value())
	{
	  int before_character = std::get<0>(*layer_change);
	  assert(0 <= before_character);
	  assert(before_character < layer_shape);
	  assert(linear_bound_in.check_fixed(layer, before_character));

	  int after_character = std::get<1>(*layer_change);
	  assert(0 <= after_character);
	  assert(after_character < layer_shape);

          build_layer(layer, layer_size, [&](dfa_state_t state_in, dfa_state_t *transitions_out)
          {
            // set all transitions to reject
            for(int i = 0; i < layer_shape; ++i)
              {
                transitions_out[i] = 0;
              }

            // then fix after value
	    DFATransitionsReference transitions_in = dfa_in.get_transitions(layer, state_in);
            transitions_out[after_character] = transitions_in[before_character];
          });
	}
      else
	{
	  // copy all transitions for each state
	  copy_layer(layer, dfa_in);
	}
      assert(this->get_layer_size(layer) == layer_size);
    }

  // done

  this->set_initial_state(dfa_in.get_initial_state());
}
