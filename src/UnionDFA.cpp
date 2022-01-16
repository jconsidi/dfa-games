// UnionDFA.cpp

#include "UnionDFA.h"

#include <algorithm>
#include <iostream>
#include <sstream>

template<int ndim, int... shape_pack>
std::vector<const DFA<ndim, shape_pack...> *> UnionDFA<ndim, shape_pack...>::convert_inputs(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>& dfas_in)
{
  std::vector<const DFA<ndim, shape_pack...> *> output;
  int num_inputs = dfas_in.size();
  for(int i = 0; i < num_inputs; ++i)
    {
      output.push_back(dfas_in[i].get());
    }
  return output;
}

template<int ndim, int... shape_pack>
uint64_t UnionDFA<ndim, shape_pack...>::union_internal(int layer, std::vector<uint64_t> union_states)
{
  if(layer < ndim)
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

  if(layer >= ndim)
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
      const int layer_shape = this->get_layer_shape(layer);

      if((ndim > 20) && (layer <= 14))
	{
	  std::cerr << "  flattening " << layer << ":" << key << std::endl;
	}

      std::vector<uint64_t> *next_transitions = new std::vector<uint64_t>[layer_shape];
      for(int i = 0; i < union_states.size(); ++i)
	{
	  const DFATransitions& union_transitions = dedupe_temp->get_transitions(layer, union_states.at(i));
	  for(int j = 0; j < layer_shape; ++j)
	    {
	      next_transitions[j].push_back(union_transitions[j]);
	    }
	}

      union_cache[layer][key] = this->add_state(layer, [this,layer,next_transitions](int i){return union_internal(layer + 1, next_transitions[i]);});
      delete[] next_transitions;
    }

  return union_cache[layer][key];
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA()
  : dedupe_temp(new DFA<ndim, shape_pack...>()),
    union_cache(new std::unordered_map<std::string, uint64_t>[64])
{
  this->add_uniform_states();
  dedupe_temp->add_uniform_states();
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const DFA<ndim, shape_pack...>& left_in, const DFA<ndim, shape_pack...>& right_in)
  : UnionDFA(std::vector<const DFA<ndim, shape_pack...> *>({&left_in, &right_in}))
{
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const std::vector<const DFA<ndim, shape_pack...> *>& dfas_in)
  : UnionDFA()
{
  int top_shape = this->get_layer_shape(0);
  std::vector<uint64_t> *top_transitions = new std::vector<uint64_t>[top_shape];

  // combine all input DFAs into one state space for deduping
  for(int dfa_index = 0; dfa_index < dfas_in.size(); ++dfa_index)
    {
      const DFA<ndim, shape_pack...>& dfa_in = *(dfas_in.at(dfa_index));

      std::vector<uint64_t> previous_mapping({0, 1});
      for(int layer = ndim - 1; layer >= 0; --layer)
	{
	  std::vector<uint64_t> current_mapping;
	  int layer_shape = this->get_layer_shape(layer);
	  int layer_size = dfa_in.get_layer_size(layer);
	  for(int state_index = 0; state_index < layer_size; ++state_index)
	    {
	      const DFATransitions& old_transitions = dfa_in.get_transitions(layer, state_index);

	      DFATransitions new_transitions(layer_shape);
	      for(int i = 0; i < layer_shape; ++i)
		{
		  uint64_t old_transition = old_transitions[i];
		  assert(old_transition < previous_mapping.size());
		  new_transitions[i] = previous_mapping.at(old_transition);
		}

	      if(layer > 0)
		{
		  current_mapping.push_back(dedupe_temp->add_state(layer, new_transitions));
		}
	      else
		{
		  for(int i = 0; i < layer_shape; ++i)
		    {
		      top_transitions[i].push_back(new_transitions[i]);
		    }
		}
	    }
	  previous_mapping = current_mapping;
	}
    }

  this->add_state(0, [this,top_transitions](int i){return union_internal(1, top_transitions[i]);});

  // clean up temporary state

  delete[] top_transitions;

  delete[] union_cache;
  union_cache = 0;

  delete dedupe_temp;
  dedupe_temp = 0;
}

template<int ndim, int... shape_pack>
UnionDFA<ndim, shape_pack...>::UnionDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>& dfas_in)
  : UnionDFA(convert_inputs(dfas_in))
{
}

// template instantiations

#include "ChessDFA.h"
#include "TicTacToeGame.h"

template class UnionDFA<CHESS_TEMPLATE_PARAMS>;
template class UnionDFA<TEST_DFA_PARAMS>;
template class UnionDFA<TICTACTOE2_DFA_PARAMS>;
template class UnionDFA<TICTACTOE3_DFA_PARAMS>;
template class UnionDFA<TICTACTOE4_DFA_PARAMS>;
