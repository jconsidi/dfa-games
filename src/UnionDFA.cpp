// UnionDFA.cpp

#include "UnionDFA.h"

#include <algorithm>
#include <iostream>
#include <sstream>

uint64_t UnionDFA::union_internal(int layer, std::vector<uint64_t> union_states)
{
  if(layer < 64)
    {
      // internal case
      for(int i = 0; i < union_states.size(); ++i)
	{
	  assert(union_states[i] < dedupe_temp->get_layer_size(layer));
	}
    }
  else
    {
      // terminal case
      for(int i = 0; i < union_states.size(); ++i)
        {
	  assert(union_states[i] < 2);
	}
    }

  // short-circuit cases
  for(int i = 0; i < union_states.size(); ++i)
    {
      if(union_states[i] == 1)
	{
	  return 1;
	}
    }

  if(layer >= 64)
    {
      // leaf but no accept inputs
      return 0;
    }

  // construct cache key, sorting to increase hit rates

  std::sort(union_states.begin(), union_states.end());

  std::ostringstream key_builder;
  key_builder << union_states[0];
  for(int i = 1; i < union_states.size(); ++i)
    {
      key_builder << "/" << union_states[i];
    }
  std::string key = key_builder.str();

  // recursion if not in cache

  if(!union_cache[layer].count(key))
    {
      if(layer <= 14)
	{
	  std::cerr << "  flattening " << layer << ":" << key << std::endl;
	}

      std::vector<uint64_t> next_transitions[DFA_MAX] = {{}};
      for(int i = 0; i < union_states.size(); ++i)
	{
	  const DFAState& union_state(dedupe_temp->get_state(layer, union_states.at(i)));
	  for(int j = 0; j < DFA_MAX; ++j)
	    {
	      next_transitions[j].push_back(union_state.transitions[j]);
	    }
	}

      uint64_t next_states[DFA_MAX];
      for(int j = 0; j < DFA_MAX; ++j)
	{
	  next_states[j] = union_internal(layer + 1, next_transitions[j]);
	}

      union_cache[layer][key] = add_state(layer, next_states);
    }

  return union_cache[layer][key];
}

UnionDFA::UnionDFA(const DFA& left_in, const DFA& right_in)
  : UnionDFA(std::vector<const DFA *>({&left_in, &right_in}))
{
}

UnionDFA::UnionDFA(const std::vector<const DFA *> dfas_in)
  : dedupe_temp(new DFA()),
    union_cache(new std::unordered_map<std::string, uint64_t>[64])
{
  add_uniform_states();
  dedupe_temp->add_uniform_states();

  std::vector<uint64_t> top_transitions[DFA_MAX] = {{}};

  // combine all input DFAs into one state space for deduping
  for(int dfa_index = 0; dfa_index < dfas_in.size(); ++dfa_index)
    {
      const DFA& dfa_in = *(dfas_in.at(dfa_index));

      std::vector<uint64_t> previous_mapping({0, 1});
      for(int layer = 63; layer >= 0; --layer)
	{
	  std::vector<uint64_t> current_mapping;
	  int layer_size = dfa_in.get_layer_size(layer);
	  for(int state_index = 0; state_index < layer_size; ++state_index)
	    {
	      const DFAState& state_in(dfa_in.get_state(layer, state_index));

	      uint64_t next_states[DFA_MAX];
	      for(int i = 0; i < DFA_MAX; ++i)
		{
		  uint64_t old_transition = state_in.transitions[i];
		  assert(old_transition < previous_mapping.size());
		  next_states[i] = previous_mapping.at(old_transition);
		}

	      if(layer > 0)
		{
		  current_mapping.push_back(dedupe_temp->add_state(layer, next_states));
		}
	      else
		{
		  for(int i = 0; i < DFA_MAX; ++i)
		    {
		      top_transitions[i].push_back(next_states[i]);
		    }
		}
	    }
	  previous_mapping = current_mapping;
	}
    }

  uint64_t top_states[DFA_MAX];
  for(int i = 0; i < DFA_MAX; ++i)
    {
      top_states[i] = union_internal(1, top_transitions[i]);
    }
  add_state(0, top_states);

  // clean up temporary state

  delete[] union_cache;
  union_cache = 0;

  delete dedupe_temp;
  dedupe_temp = 0;
}
