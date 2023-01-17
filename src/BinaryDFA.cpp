// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <assert.h>

#include <algorithm>
#include <bit>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>

#include "BitSet.h"
#include "Flashsort.h"
#include "MemoryMap.h"
#include "Profile.h"
#include "VectorBitSet.h"

class LayerTransitions;

class FilteredLayerTransitionsIterator;

class LayerTransitionsHelper
{
protected:

  const DFA& left;
  const DFA& right;
  const int layer;

  const int curr_layer_shape;
  const size_t curr_left_size;
  const size_t curr_right_size;
  const size_t next_left_size;
  const size_t next_right_size;

public:

  LayerTransitionsHelper(const DFA& left_in,
			 const DFA& right_in,
			 int layer_in)
    : left(left_in),
      right(right_in),
      layer(layer_in),
      curr_layer_shape(left.get_layer_shape(layer)),
      curr_left_size(left.get_layer_size(layer)),
      curr_right_size(right.get_layer_size(layer)),
      next_left_size(left.get_layer_size(layer+1)),
      next_right_size(right.get_layer_size(layer + 1))
  {
  }

  dfa_state_t decode_next_left(size_t next_pair) const
  {
    return next_pair / next_right_size;
  }

  dfa_state_t decode_next_right(size_t next_pair) const
  {
    return next_pair % next_right_size;
  }

  size_t get_next_pair(size_t curr_i, int curr_j) const
  {
    assert(curr_i < curr_left_size * curr_right_size);
    dfa_state_t curr_left_state = curr_i / curr_right_size;
    dfa_state_t curr_right_state = curr_i % curr_right_size;

    DFATransitionsReference left_transitions = left.get_transitions(layer, curr_left_state);
    DFATransitionsReference right_transitions = right.get_transitions(layer, curr_right_state);

    size_t next_left_state = left_transitions.at(curr_j);
    assert(next_left_state < next_left_size);
    size_t next_right_state = right_transitions.at(curr_j);
    assert(next_right_state < next_right_size);

    return next_left_state * next_right_size + next_right_state;
  }
};

class LayerTransitionsIterator
  : public LayerTransitionsHelper
{
protected:

  BitSetIterator iter;

private:

  mutable size_t curr_i;
  int curr_j;

protected:

  LayerTransitionsIterator(const DFA& left_in,
			   const DFA& right_in,
			   int layer_in,
			   BitSetIterator iter_in)
    : LayerTransitionsHelper(left_in, right_in, layer_in),
      iter(iter_in),
      curr_i(0),
      curr_j(0)
  {
  }

public:

  size_t operator*() const
  {
    if(curr_j == 0)
      {
	curr_i = *iter;
      }

    return this->get_next_pair(curr_i, curr_j);
  }

  LayerTransitionsIterator& operator++()
  {
    // prefix ++
    curr_j = (curr_j + 1) % this->curr_layer_shape;
    if(curr_j == 0)
      {
	++iter;
      }
    return *this;
  }

  bool operator<(const LayerTransitionsIterator& right_in) const
  {
    if(iter < right_in.iter)
      {
	return true;
      }
    else if(right_in.iter < iter)
      {
	return false;
      }

    return curr_j < right_in.curr_j;
  }

  friend LayerTransitions;
};

class FilteredLayerTransitionsIterator
  : public LayerTransitionsIterator
{
  const BitSetIterator end;
  std::function<bool(dfa_state_t, dfa_state_t)> filter_func;

  FilteredLayerTransitionsIterator(const DFA& left_in,
				   const DFA& right_in,
				   int layer_in,
				   BitSetIterator begin_in,
				   BitSetIterator end_in,
				   std::function<bool(dfa_state_t, dfa_state_t)> filter_in)
    : LayerTransitionsIterator(left_in, right_in, layer_in, begin_in),
      end(end_in),
      filter_func(filter_in)
  {
    if(check_filter())
      {
	++(*this);
      }
  }

public:

  bool check_filter()
  {
    if(!(this->iter < end))
      {
	return false;
      }

    size_t pair = *(*this);

    dfa_state_t left = this->decode_next_left(pair);
    dfa_state_t right = this->decode_next_right(pair);

    return filter_func(left, right);
  }

  FilteredLayerTransitionsIterator& operator++()
  {
    LayerTransitionsIterator *this2 = this;
    ++(*this2);
    while(check_filter())
      {
	++(*this2);
      }

    return *this;
  }

  friend LayerTransitions;
};

class LayerTransitions
{
  const DFA& left;
  const DFA& right;
  int layer;
  const BitSet& layer_pairs;

public:

  LayerTransitions(const DFA& left_in,
		   const DFA& right_in,
		   int layer_in,
		   const BitSet& layer_pairs_in)
    : left(left_in),
      right(right_in),
      layer(layer_in),
      layer_pairs(layer_pairs_in)
  {
  }

  LayerTransitionsIterator cbegin() const
  {
    return LayerTransitionsIterator(left, right, layer, layer_pairs.cbegin());
  }

  FilteredLayerTransitionsIterator cbegin_filtered(std::function<bool(dfa_state_t, dfa_state_t)> filter_func) const
  {
    return FilteredLayerTransitionsIterator(left, right, layer, layer_pairs.cbegin(), layer_pairs.cend(), filter_func);
  }

  LayerTransitionsIterator cend() const
  {
    return LayerTransitionsIterator(left, right, layer, layer_pairs.cend());
  }

  FilteredLayerTransitionsIterator cend_filtered() const
  {
    return FilteredLayerTransitionsIterator(left, right, layer, layer_pairs.cend(), layer_pairs.cend(), [](dfa_state_t, dfa_state_t){return false;});
  }
};

BinaryDFA::BinaryDFA(const DFA& left_in,
		     const DFA& right_in,
		     const BinaryFunction& leaf_func)
  : DFA(left_in.get_shape())
{
  assert(left_in.get_shape() == right_in.get_shape());

  // both inputs constant
  if((left_in.get_initial_state() < 2) && (right_in.get_initial_state() < 2))
    {
      this->set_initial_state(leaf_func(left_in.get_initial_state(),
					right_in.get_initial_state()));
      return;
    }

  // constant sink cases
  for(int i = 0; i < 2; ++i)
    {
      if(left_in.is_constant(i) && leaf_func.has_left_sink(i))
	{
	  this->set_initial_state(i);
	  return;
	}

      if(right_in.is_constant(i) && leaf_func.has_right_sink(i))
	{
	  this->set_initial_state(i);
	  return;
	}
    }

  // linear cases
  if(left_in.is_linear() &&
     leaf_func.has_left_sink(0))
    {
      // left side is linear DFA
      build_linear(left_in, right_in, leaf_func);
      return;
    }
  else if(right_in.is_linear() &&
	  leaf_func.is_commutative() &&
	  leaf_func.has_right_sink(0))
    {
      // right side is linear DFA and confirmed commutative property
      build_linear(right_in, left_in, leaf_func);
      return;
    }

  // quadratic default
  build_quadratic_mmap(left_in, right_in, leaf_func);
}

static std::string binary_build_file_prefix(int layer)
{
  std::ostringstream filename_builder;
  filename_builder << "scratch/binarydfa/layer=" << (layer < 9 ? "0" : "") << (layer + 1);
  return filename_builder.str();
}

void BinaryDFA::build_linear(const DFA& left_in,
			     const DFA& right_in,
			     const BinaryFunction& leaf_func)
{
  Profile profile("build_linear");

  assert(left_in.is_linear());
  assert(leaf_func.has_left_sink(0));

  // 0. identify left states used, and which transitions are kept.
  //
  // 1. forward pass to identify all right states reachable from
  // initial state.
  //
  // 2. backward pass rewriting states reachable in forward pass.

  // identify left states used

  std::vector<dfa_state_t> left_states = {left_in.get_initial_state()};
  std::vector<std::vector<bool>> left_filters;
  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      int layer_shape = left_in.get_layer_shape(layer);
      left_filters.emplace_back(layer_shape);

      DFATransitionsReference left_transitions = left_in.get_transitions(layer, left_states.at(layer));

      dfa_state_t next_left_state = 0;
      for(int i = 0; i < layer_shape; ++i)
	{
	  dfa_state_t left_temp = left_transitions[i];
	  if(left_temp != 0)
	    {
	      next_left_state = left_temp;
	      left_filters[layer][i] = true;
	    }
	}
      assert(next_left_state);
      left_states.push_back(next_left_state);
    }
  assert(left_states.size() == get_shape_size() + 1);
  assert(left_states[get_shape_size()] == 1);
  assert(left_filters.size() == get_shape_size());

  ////////////////////////////////////////////////////////////
  // forward pass finding right reachable states /////////////
  ////////////////////////////////////////////////////////////

  std::vector<VectorBitSet> right_reachable;

  // first layer has just the initial state reachable
  right_reachable.emplace_back(right_in.get_layer_size(0));
  right_reachable[0].add(right_in.get_initial_state());

  // expand each layer's reachable states to find next layer's
  // reachable states.
  for(int layer = 0; layer < get_shape_size() - 1; ++layer)
    {
      int layer_shape = right_in.get_layer_shape(layer);

      right_reachable.emplace_back(right_in.get_layer_size(layer+1));
      VectorBitSet& next_reachable = right_reachable.back();

      for(dfa_state_t curr_state: std::as_const(right_reachable[layer]))
	{
	  DFATransitionsReference right_transitions = right_in.get_transitions(layer, curr_state);
	  for(int i = 0; i < layer_shape; ++i)
	    {
	      if(left_filters[layer][i])
		{
		  next_reachable.add(right_transitions[i]);
		}
	    }
	}
    }
  assert(right_reachable.size() == get_shape_size());

  // prune if nothing reachable at the end

  if(right_reachable[get_shape_size() - 1].count() == 0)
    {
      this->set_initial_state(0);
      return;
    }

  // expand dummy layer for terminal reject / accept
  right_reachable.emplace_back(2);
  right_reachable[get_shape_size()].add(0);
  right_reachable[get_shape_size()].add(1);

  ////////////////////////////////////////////////////////////
  // backward pass rewriting states //////////////////////////
  ////////////////////////////////////////////////////////////

  std::vector<MemoryMap<dfa_state_t>> changed_states;
  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      changed_states.emplace_back(right_reachable.at(layer).count());
    }

  // terminal pseudo-layer
  changed_states.emplace_back(2);
  changed_states[get_shape_size()][0] = leaf_func(1, 0);
  changed_states[get_shape_size()][1] = leaf_func(1, 1);

  // backward pass
  for(int layer = get_shape_size() - 1; layer >= 0; --layer)
    {
      VectorBitSetIndex next_index(right_reachable[layer+1]);

      int layer_shape = this->get_layer_shape(layer);

      std::function<dfa_state_t(dfa_state_t)> get_next_changed = [&](dfa_state_t next_state_in)
      {
	assert(right_reachable[layer+1].check(next_state_in));
	return changed_states[layer+1][next_index.rank(next_state_in)];
      };

      DFATransitionsStaging transitions_temp(layer_shape, 0);

      dfa_state_t state_out = 0;
      // rewrite all transitions for each state
      for(dfa_state_t state_in : right_reachable[layer])
	{
	  DFATransitionsReference transitions_in = right_in.get_transitions(layer, state_in);
	  for(int i = 0; i < layer_shape; ++i)
	    {
	      if(left_filters[layer][i])
		{
		  transitions_temp[i] = get_next_changed(transitions_in[i]);
		}
	    }
	  changed_states[layer][state_out++] = this->add_state(layer, transitions_temp);
	}
      assert(state_out == changed_states[layer].size());

      changed_states.pop_back();
      assert(changed_states.size() == layer + 1);
    }
  assert(changed_states.size() == 1);

  // done

  this->set_initial_state(changed_states[0][0]);
}

void BinaryDFA::build_quadratic_mmap(const DFA& left_in,
				     const DFA& right_in,
				     const BinaryFunction& leaf_func)
{
  Profile profile("build_quadratic_mmap");

  // identify cases where leaf_func allows full evaluation without
  // going to leaves...

  dfa_state_t left_sink = ~dfa_state_t(0);
  if(leaf_func.has_left_sink(0))
    {
      left_sink = 0;
    }
  else if(leaf_func.has_left_sink(1))
    {
      left_sink = 1;
    }
  dfa_state_t right_sink = ~dfa_state_t(0);
  if(leaf_func.has_right_sink(0))
    {
      right_sink = 0;
    }
  else if(leaf_func.has_right_sink(1))
    {
      right_sink = 1;
    }

  // apply shortcircuit logic to previously detected cases
  std::function<dfa_state_t(dfa_state_t, dfa_state_t)> shortcircuit_func = [=](dfa_state_t left_in, dfa_state_t right_in) -> dfa_state_t
  {
    if((left_in < 2) && (right_in < 2))
      {
	// constant inputs
	return leaf_func(left_in, right_in);
      }

    if(left_in == left_sink)
      {
	return left_in;
      }

    if(right_in == right_sink)
      {
	return right_in;
      }

    // do not use this function unless shortcircuit evaluation
    assert(0);
  };

  // decide whether shortcircuit evaluation applies and we can filter
  // out these pairs from further evaluation.
  std::function<bool(dfa_state_t, dfa_state_t)> filter_func = [=](dfa_state_t left_in, dfa_state_t right_in)
  {
    if((left_in < 2) && (right_in < 2))
      {
	return true;
      }

    if(left_in == left_sink)
      {
	return true;
      }

    if(right_in == right_sink)
      {
	return true;
      }

    return false;
  };

  std::vector<BitSet> pairs_by_layer;
  pairs_by_layer.reserve(get_shape_size() + 1);

  pairs_by_layer.emplace_back(left_in.get_layer_size(0) * right_in.get_layer_size(0));

  dfa_state_t initial_left = left_in.get_initial_state();
  dfa_state_t initial_right = right_in.get_initial_state();
  if(filter_func(initial_left, initial_right))
    {
      this->set_initial_state(shortcircuit_func(initial_left, initial_right));
      return;
    }

  size_t initial_pair = initial_left * right_in.get_layer_size(0) + initial_right;
  pairs_by_layer[0].prepare(initial_pair);
  pairs_by_layer[0].allocate();
  pairs_by_layer[0].add(initial_pair);

  // forward pass

  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      profile.set_prefix("layer=" + std::to_string(layer));
      profile.tic("forward init");

      // get size of current layer and next layer.
      //
      // using size_t for product calculations...

      size_t next_left_size = left_in.get_layer_size(layer + 1);
      size_t next_right_size = right_in.get_layer_size(layer + 1);

      profile.tic("forward mmap");

      assert(pairs_by_layer.size() == layer + 1);
      size_t next_size = next_left_size * next_right_size;
      bool disk_mmap = next_size >= 1ULL << 32;
      if(disk_mmap)
	{
	  pairs_by_layer.emplace_back(binary_build_file_prefix(layer), next_size);
	}
      else
	{
	  pairs_by_layer.emplace_back(next_size);
	}
      assert(pairs_by_layer.size() == layer + 2);

      const BitSet& curr_layer = pairs_by_layer.at(layer);
      BitSet& next_layer = pairs_by_layer.at(layer + 1);

      profile.tic("forward pairs");

      LayerTransitions layer_transitions(left_in, right_in, layer, curr_layer);

      populate_bitset<FilteredLayerTransitionsIterator>(next_layer,
							layer_transitions.cbegin_filtered(filter_func),
							layer_transitions.cend_filtered());

      if(disk_mmap)
	{
	  size_t bits_set = next_layer.count();
	  size_t bits_total = next_layer.size();
	  std::cout << "bits set = " << bits_set << " / " << bits_total << " ~ " << (double(bits_set) / double(bits_total)) << std::endl;
	}

      if(next_layer.count() == 0)
	{
	  // all pairs in next layer were filtered. no need to continue.
	  break;
	}
    }

  assert(pairs_by_layer.size() > 0);
  assert(pairs_by_layer.back().count() == 0);

  // apply leaf function

  profile.set_prefix("");
  profile.tic("leaves");

  // initially empty since shortcircuiting will happen by last layer.
  std::vector<dfa_state_t> next_pair_mapping;

  // backward pass

  for(int layer = pairs_by_layer.size() - 2; layer >= 0; --layer)
    {
      profile.set_prefix("layer=" + std::to_string(layer));
      profile.tic("backward init");

      assert(pairs_by_layer.size() == layer + 2);

      int curr_layer_shape = this->get_layer_shape(layer);
      const BitSet& curr_layer = pairs_by_layer.at(layer);
      std::vector<dfa_state_t> curr_pair_mapping;

      profile.tic("backward curr index");

      BitSetIndex curr_index(curr_layer);

      profile.tic("backward next index");

      const BitSet& next_layer = pairs_by_layer.at(layer + 1);

      BitSetIndex next_index(next_layer);

      profile.tic("backward enumerate");

      LayerTransitionsHelper helper(left_in, right_in, layer);

      size_t curr_layer_count = curr_layer.count();
      MemoryMap<size_t> curr_pairs = (curr_layer_count * sizeof(size_t) < 1ULL << 32)
	? MemoryMap<size_t>(curr_layer_count)
	: MemoryMap<size_t>(binary_build_file_prefix(layer) + "-sort", curr_layer_count);

      size_t curr_pairs_k = 0;
      for(auto iter = curr_layer.cbegin();
	  iter < curr_layer.cend();
	  ++curr_pairs_k, ++iter)
	{
	  curr_pairs[curr_pairs_k] = *iter;
	}
      assert(curr_pairs_k == curr_pairs.size());

      profile.tic("backward transitions");

      auto get_next_state = [&](size_t curr_pair, int curr_j)
      {
	assert(curr_layer.check(curr_pair));
	size_t next_pair = helper.get_next_pair(curr_pair, curr_j);
	dfa_state_t next_left = helper.decode_next_left(next_pair);
	dfa_state_t next_right = helper.decode_next_right(next_pair);

	if(filter_func(next_left, next_right))
	  {
	    return shortcircuit_func(next_left, next_right);
	  }
	else
	  {
	    dfa_state_t next_compact = next_index.rank(next_pair);
	    assert(next_compact < next_pair_mapping.size());
	    dfa_state_t next_deduped = next_pair_mapping.at(next_compact);
	    return next_deduped;
	  }
      };

      // TODO : spill to disk if too big
      MemoryMap<dfa_state_t> curr_transitions(curr_layer_count * curr_layer_shape);
      for(dfa_state_t i = 0; i < curr_layer_count; ++i)
	{
	  for(int j = 0; j < curr_layer_shape; ++j)
	    {
	      curr_transitions[i * curr_layer_shape + j] = get_next_state(curr_pairs[i], j);
	    }
	}

      profile.tic("backward sort");

      MemoryMap<dfa_state_t> curr_pairs_permutation(curr_layer_count);
      for(dfa_state_t i = 0; i < curr_layer_count; ++i)
	{
	  curr_pairs_permutation[i] = i;
	}

      auto compare_pair = [&](size_t curr_pair_index_a, size_t curr_pair_index_b)
      {
	return std::memcmp(&(curr_transitions[curr_pair_index_a * curr_layer_shape]),
			   &(curr_transitions[curr_pair_index_b * curr_layer_shape]),
			   sizeof(dfa_state_t) * curr_layer_shape) < 0;
      };

      // make permutation of pairs sorted by transitions
      std::sort(curr_pairs_permutation.begin(), curr_pairs_permutation.end(), compare_pair);

      profile.tic("backward states");

      curr_pair_mapping.resize(curr_layer_count);

      // figure out first two states used
      dfa_state_t curr_logical = ~dfa_state_t(0);
      dfa_state_t next_logical = 2; // next state to be set

      auto set_helper = [&](size_t curr_pairs_sorted_index)
      {
	DFATransitionsStaging set_transitions;
	for(int j = 0; j < curr_layer_shape; ++j)
	  {
	    set_transitions.push_back(curr_transitions[curr_pairs_permutation[curr_pairs_sorted_index] * curr_layer_shape + j]);
	  }

	if(set_transitions.at(0) < 2)
	  {
	    // possible accept or reject state
	    bool uniform_transitions = true;
	    for(int j = 1; j < curr_layer_shape; ++j)
	      {
		if(set_transitions.at(j) != set_transitions.at(0))
		  {
		    uniform_transitions = false;
		    break;
		  }
	      }

	    if(uniform_transitions)
	      {
		// confirmed accept or reject state
		curr_logical = set_transitions[0];
		return;
	      }
	  }

	curr_logical = next_logical;
	++next_logical;

	dfa_state_t new_state = this->add_state(layer, set_transitions);
	assert(new_state == curr_logical);
      };

      set_helper(0);

      curr_pair_mapping[curr_index.rank(curr_pairs[curr_pairs_permutation[0]])] = curr_logical;

      for(size_t k = 1; k < curr_layer_count; ++k)
	{
	  if(compare_pair(curr_pairs_permutation[k-1], curr_pairs_permutation[k]))
	    {
	      set_helper(k);
	    }
	  curr_pair_mapping[curr_index.rank(curr_pairs[curr_pairs_permutation[k]])] = curr_logical;
	}

      assert(curr_pair_mapping.size() == curr_layer_count);

      next_pair_mapping.clear();
      std::swap(next_pair_mapping, curr_pair_mapping);

      assert(next_pair_mapping.size() > 0);
      assert(curr_pair_mapping.size() == 0);

      // shrink state

      profile.tic("backward munmap");
      pairs_by_layer.pop_back();

      // cleanup in destructors
      profile.tic("backward cleanup");
    }
  profile.set_prefix("");

  assert(pairs_by_layer.size() == 1);
  assert(next_pair_mapping.size() == 1);
  this->set_initial_state(next_pair_mapping[0]);

  assert(this->ready());
  profile.tic("final cleanup");
}
