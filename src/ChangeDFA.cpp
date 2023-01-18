// ChangeDFA.cpp

#include "ChangeDFA.h"

#include <functional>

#include "MemoryMap.h"
#include "Profile.h"
#include "VectorBitSet.h"

ChangeDFA::ChangeDFA(const DFA& dfa_in,
		     const change_vector& changes_in)
  : DFA(dfa_in.get_shape())
{
  assert(changes_in.size() == dfa_in.get_shape_size());

  const DFALinearBound& linear_bound_in = dfa_in.get_linear_bound();
  bool pre_condition_holds = true;
  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      change_optional layer_change = changes_in[layer];
      if(layer_change.has_value())
	{
	  int before_character = std::get<0>(*layer_change);
	  if(!linear_bound_in.check_fixed(layer, before_character))
	    {
	      pre_condition_holds = false;
	      break;
	    }
	}
    }

  if(pre_condition_holds)
    {
      build_one_pass(dfa_in, changes_in);
    }
  else
    {
      build_two_pass(dfa_in, changes_in);
    }
}

void ChangeDFA::build_one_pass(const DFA& dfa_in, const change_vector& changes_in)
{
  Profile profile("build_one_pass");

  // rewrites DFA in one pass assuming that all states have been
  // restricted to match the change pre-conditions. this assumption
  // will be confirmed via the input DFA's linear bounds.

  const DFALinearBound& linear_bound_in = dfa_in.get_linear_bound();

  // will be reachable after changes and rewrite
  // them all in one pass without checking. the DFA will still match
  // the right strings if this assumption is not correct, but will be
  // bigger than necessary.

  for(int layer = get_shape_size() - 1; layer >= 0; --layer)
    {
      int layer_shape = this->get_layer_shape(layer);
      dfa_state_t layer_size = dfa_in.get_layer_size(layer);

      change_optional layer_change = changes_in[layer];

      DFATransitionsStaging transitions_temp(layer_shape, 0);

      if(layer_change.has_value())
	{
	  int before_character = std::get<0>(*layer_change);
	  assert(0 <= before_character);
	  assert(before_character < layer_shape);
	  assert(linear_bound_in.check_fixed(layer, before_character));

	  int after_character = std::get<1>(*layer_change);
	  assert(0 <= after_character);
	  assert(after_character < layer_shape);

	  // transitions_temp is all zeros, and will repeatedly
	  // rewrite the same "after" transition.

	  for(dfa_state_t state_in = 2; state_in < layer_size; ++state_in)
	    {
	      DFATransitionsReference transitions_in = dfa_in.get_transitions(layer, state_in);
	      transitions_temp[after_character] = transitions_in[before_character];
	      dfa_state_t state_out = this->add_state(layer, transitions_temp);
	      assert(state_out == state_in);
	    }
	}
      else
	{
	  // copy all transitions for each state
	  for(dfa_state_t state_in = 2; state_in < layer_size; ++state_in)
	    {
	      DFATransitionsReference transitions_in = dfa_in.get_transitions(layer, state_in);
	      dfa_state_t state_out = this->add_state_by_reference(layer, transitions_in);
	      assert(state_out == state_in);
	    }
	}
      assert(this->get_layer_size(layer) == layer_size);
    }

  // done

  this->set_initial_state(dfa_in.get_initial_state());
}

void ChangeDFA::build_two_pass(const DFA& dfa_in, const change_vector& changes_in)
{
  // 1. forward pass to identify all states reachable from initial
  // state.
  //
  // 2. backward pass rewriting states reachable in forward pass.

  Profile profile("build_two_pass");

  ////////////////////////////////////////////////////////////
  // forward pass finding reachable states ///////////////////
  ////////////////////////////////////////////////////////////

  std::vector<VectorBitSet> forward_reachable;

  // first layer has just the initial state reachable
  forward_reachable.emplace_back(dfa_in.get_layer_size(0));
  forward_reachable[0].add(dfa_in.get_initial_state());

  // expand each layer's reachable states to find next layer's
  // reachable states.
  for(int layer = 0; layer < get_shape_size() - 1; ++layer)
    {
      forward_reachable.emplace_back(dfa_in.get_layer_size(layer+1));
      VectorBitSet& next_reachable = forward_reachable.back();

      int layer_shape = dfa_in.get_layer_shape(layer);
      change_optional layer_change = changes_in[layer];

      if(layer_change.has_value())
	{
	  int before_character = std::get<0>(*layer_change);
	  assert(0 <= before_character);
	  assert(before_character < layer_shape);

	  for(dfa_state_t curr_state: std::as_const(forward_reachable[layer]))
	    {
	      DFATransitionsReference curr_transitions = dfa_in.get_transitions(layer, dfa_state_t(curr_state));
	      next_reachable.add(curr_transitions[before_character]);
	    }
	}
      else
	{
	  for(dfa_state_t curr_state: std::as_const(forward_reachable[layer]))
	    {
	      DFATransitionsReference curr_transitions = dfa_in.get_transitions(layer, dfa_state_t(curr_state));
	      for(int i = 0; i < layer_shape; ++i)
		{
		  next_reachable.add(curr_transitions[i]);
		}
	    }
	}
    }

  assert(forward_reachable.size() == get_shape_size());

  // prune if nothing reachable at the end

  if(forward_reachable[get_shape_size() - 1].count() == 0)
    {
      this->set_initial_state(0);
      return;
    }

  // expand dummy layer for terminal reject / accept
  forward_reachable.emplace_back(2);
  forward_reachable[get_shape_size()].add(0);
  forward_reachable[get_shape_size()].add(1);

  ////////////////////////////////////////////////////////////
  // backward pass rewriting states //////////////////////////
  ////////////////////////////////////////////////////////////

  std::vector<MemoryMap<dfa_state_t>> changed_states;
  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      changed_states.emplace_back(forward_reachable.at(layer).count());
    }

  // terminal pseudo-layer
  changed_states.emplace_back(2);
  changed_states[get_shape_size()][0] = 0;
  changed_states[get_shape_size()][1] = 1;

  // backward pass
  for(int layer = get_shape_size() - 1; layer >= 0; --layer)
    {
      VectorBitSetIndex next_index(forward_reachable[layer+1]);

      int layer_shape = this->get_layer_shape(layer);

      std::function<dfa_state_t(dfa_state_t)> get_next_changed = [&](dfa_state_t next_state_in)
      {
	assert(forward_reachable[layer+1].check(next_state_in));
	return changed_states[layer+1][next_index.rank(next_state_in)];
      };

      change_optional layer_change = changes_in[layer];

      DFATransitionsStaging transitions_temp(layer_shape, 0);

      dfa_state_t state_out = 0;
      if(layer_change.has_value())
	{
	  int before_character = std::get<0>(*layer_change);
	  assert(0 <= before_character);
	  assert(before_character < layer_shape);

	  int after_character = std::get<1>(*layer_change);
	  assert(0 <= after_character);
	  assert(after_character < layer_shape);

	  // transitions_temp is all zeros, and will repeatedly
	  // rewrite the same "after" transition.
	  for(dfa_state_t state_in : forward_reachable[layer])
	    {
	      DFATransitionsReference transitions_in = dfa_in.get_transitions(layer, state_in);
	      transitions_temp[after_character] = get_next_changed(transitions_in[before_character]);
	      changed_states[layer][state_out++] = this->add_state(layer, transitions_temp);
	    }
	}
      else
	{
	  // rewrite all transitions for each state
	  for(dfa_state_t state_in : forward_reachable[layer])
	    {
	      DFATransitionsReference transitions_in = dfa_in.get_transitions(layer, state_in);
	      for(int i = 0; i < layer_shape; ++i)
		{
		  transitions_temp[i] = get_next_changed(transitions_in[i]);
		}
	      changed_states[layer][state_out++] = this->add_state(layer, transitions_temp);
	    }
	}
      assert(state_out == changed_states[layer].size());

      changed_states.pop_back();
      assert(changed_states.size() == layer + 1);
    }
  assert(changed_states.size() == 1);

  // done

  this->set_initial_state(changed_states[0][0]);
}
