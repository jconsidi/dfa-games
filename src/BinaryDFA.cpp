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

template <int ndim, int... shape_pack>
class LayerTransitionsIterator;

template <int ndim, int... shape_pack>
class FilteredLayerTransitionsIterator;

template <int ndim, int... shape_pack>
class LayerTransitions
{
  const DFA<ndim, shape_pack...>& left;
  const DFA<ndim, shape_pack...>& right;
  int layer;
  const BitSet& layer_pairs;

public:

  LayerTransitions(const DFA<ndim, shape_pack...>& left_in,
		   const DFA<ndim, shape_pack...>& right_in,
		   int layer_in,
		   const BitSet& layer_pairs_in)
    : left(left_in),
      right(right_in),
      layer(layer_in),
      layer_pairs(layer_pairs_in)
  {
  }

  LayerTransitionsIterator<ndim, shape_pack...> cbegin() const
  {
    return LayerTransitionsIterator<ndim, shape_pack...>(left, right, layer, layer_pairs.cbegin());
  }

  FilteredLayerTransitionsIterator<ndim, shape_pack...> cbegin_filtered(std::function<bool(dfa_state_t, dfa_state_t)> filter_func) const
  {
    return FilteredLayerTransitionsIterator<ndim, shape_pack...>(left, right, layer, layer_pairs.cbegin(), layer_pairs.cend(), filter_func);
  }

  LayerTransitionsIterator<ndim, shape_pack...> cend() const
  {
    return LayerTransitionsIterator<ndim, shape_pack...>(left, right, layer, layer_pairs.cend());
  }

  FilteredLayerTransitionsIterator<ndim, shape_pack...> cend_filtered() const
  {
    return FilteredLayerTransitionsIterator<ndim, shape_pack...>(left, right, layer, layer_pairs.cend(), layer_pairs.cend(), [](dfa_state_t, dfa_state_t){return false;});
  }

  friend LayerTransitionsIterator<ndim, shape_pack...>;
};

template <int ndim, int... shape_pack>
class LayerTransitionsHelper
{
protected:

  const DFA<ndim, shape_pack...>& left;
  const DFA<ndim, shape_pack...>& right;
  const int layer;

  const int curr_layer_shape;
  const size_t curr_left_size;
  const size_t curr_right_size;
  const size_t next_left_size;
  const size_t next_right_size;

public:

  LayerTransitionsHelper(const DFA<ndim, shape_pack...>& left_in,
			 const DFA<ndim, shape_pack...>& right_in,
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

template <int ndim, int... shape_pack>
class LayerTransitionsIterator
  : public LayerTransitionsHelper<ndim, shape_pack...>
{
protected:

  BitSetIterator iter;

private:

  mutable size_t curr_i;
  int curr_j;

protected:

  LayerTransitionsIterator(const DFA<ndim, shape_pack...>& left_in,
			   const DFA<ndim, shape_pack...>& right_in,
			   int layer_in,
			   BitSetIterator iter_in)
    : LayerTransitionsHelper<ndim, shape_pack...>(left_in, right_in, layer_in),
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

  friend LayerTransitions<ndim, shape_pack...>;
};

template <int ndim, int... shape_pack>
class FilteredLayerTransitionsIterator
  : public LayerTransitionsIterator<ndim, shape_pack...>
{
  const BitSetIterator end;
  std::function<bool(dfa_state_t, dfa_state_t)> filter_func;

  FilteredLayerTransitionsIterator(const DFA<ndim, shape_pack...>& left_in,
				   const DFA<ndim, shape_pack...>& right_in,
				   int layer_in,
				   BitSetIterator begin_in,
				   BitSetIterator end_in,
				   std::function<bool(dfa_state_t, dfa_state_t)> filter_in)
    : LayerTransitionsIterator<ndim, shape_pack...>(left_in, right_in, layer_in, begin_in),
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
    LayerTransitionsIterator<ndim, shape_pack...> *this2 = this;
    ++(*this2);
    while(check_filter())
      {
	++(*this2);
      }

    return *this;
  }

  friend LayerTransitions<ndim, shape_pack...>;
};

template <int ndim, int... shape_pack>
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const DFA<ndim, shape_pack...>& left_in,
					  const DFA<ndim, shape_pack...>& right_in,
					  leaf_func_t leaf_func)
  : ExplicitDFA<ndim, shape_pack...>()
{
  binary_build(left_in, right_in, leaf_func);
}

template <int ndim, int... shape_pack>
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>& dfas_in,
					  leaf_func_t leaf_func)
  : ExplicitDFA<ndim, shape_pack...>()
{
  // confirm commutativity
  assert(leaf_func(0, 1) == leaf_func(1, 0));

  // special case 0-2 input DFAs
  switch(dfas_in.size())
    {
    case 0:
      {
	throw std::logic_error("zero dfas passed to BinaryDFA vector constructor");
      }

    case 1:
      {
	// copy singleton input
	std::shared_ptr<const DFA<ndim, shape_pack...>> source = dfas_in[0];
	for(int layer = ndim - 1; layer >= 0; --layer)
	  {
	    dfa_state_t layer_size = source->get_layer_size(layer);
	    for(dfa_state_t state_index = 2; state_index < layer_size; ++state_index)
	      {
		DFATransitionsReference transitions(source->get_transitions(layer, state_index));
		this->set_state(layer, state_index, transitions);
	      }
	  }
	this->set_initial_state(source->get_initial_state());
	return;
      }

    case 2:
      {
	// simple binary merge
	binary_build(*(dfas_in[0]), *(dfas_in[1]), leaf_func);
	return;
      }
    }

  // three or more DFAs. will greedily merge the two smallest DFAs
  // counting states until two DFAs left, then merge the last two for
  // this DFA.

  typedef struct reverse_less
  {
    bool operator()(std::shared_ptr<const DFA<ndim, shape_pack...>> a,
		    std::shared_ptr<const DFA<ndim, shape_pack...>> b) const
    {
      return a->states() > b->states();
    }
  } reverse_less;

  std::priority_queue<
    std::shared_ptr<const DFA<ndim, shape_pack...>>,
    std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>,
    reverse_less> dfa_queue;

  for(int i = 0; i < dfas_in.size(); ++i)
    {
      dfa_queue.push(dfas_in[i]);
    }

  while(dfa_queue.size() > 2)
    {
      std::shared_ptr<const DFA<ndim, shape_pack...>> last = dfa_queue.top();
      dfa_queue.pop();
      std::shared_ptr<const DFA<ndim, shape_pack...>> second_last = dfa_queue.top();
      dfa_queue.pop();

      if(second_last->states() >= 1024)
	{
	  std::cerr << "  merging DFAs with " << last->states() << " states and " << second_last->states() << " states (" << dfa_queue.size() << " remaining)" << std::endl;
	}
      dfa_queue.push(std::shared_ptr<const DFA<ndim, shape_pack...>>(new BinaryDFA(*second_last, *last, leaf_func)));
    }
  assert(dfa_queue.size() == 2);

  std::shared_ptr<const DFA<ndim, shape_pack...>> d0 = dfa_queue.top();
  dfa_queue.pop();
  std::shared_ptr<const DFA<ndim, shape_pack...>> d1 = dfa_queue.top();
  dfa_queue.pop();
  assert(dfa_queue.size() == 0);

  if((d0->states() >= 1024) || (d1->states() >= 1024))
    {
      std::cerr << "  merging DFAs with " << d0->states() << " states and " << d1->states() << " states (final)" << std::endl;
    }

  binary_build(*d0, *d1, leaf_func);
  assert(this->ready());

  if(this->states() >= 1024)
    {
      std::cerr << "  merged DFA has " << this->states() << " states and " << this->size() << " positions" << std::endl;
    }
}

static std::string binary_build_file_prefix(int layer)
{
  std::ostringstream filename_builder;
  filename_builder << "/tmp/chess/binarydfa/layer=" << (layer < 9 ? "0" : "") << (layer + 1);
  return filename_builder.str();
}

template <int ndim, int... shape_pack>
void BinaryDFA<ndim, shape_pack...>::binary_build(const DFA<ndim, shape_pack...>& left_in,
						  const DFA<ndim, shape_pack...>& right_in,
						  leaf_func_t leaf_func)
{
  Profile profile("binary_build");

  // identify cases where leaf_func allows full evaluation without
  // going to leaves...

  dfa_state_t left_sink = ~dfa_state_t(0);
  if((leaf_func(0, 0) == 0) && (leaf_func(0, 1) == 0))
    {
      left_sink = 0;
    }
  else if((leaf_func(1, 0) == 1) && (leaf_func(1, 1) == 1))
    {
      left_sink = 1;
    }
  // dfa_state_t right_sink = ~dfa_state_t(0);

  // apply shortcircuit logic to previously detected cases
  std::function<dfa_state_t(dfa_state_t, dfa_state_t)> shortcircuit_func = [=](dfa_state_t left_in, dfa_state_t right_in)
  {
    if(left_in == left_sink)
      {
	return left_in;
      }

    // do not use this function unless shortcircuit evaluation
    assert(0);
  };

  // decide whether shortcircuit evaluation applies and we can filter
  // out these pairs from further evaluation.
  std::function<bool(dfa_state_t, dfa_state_t)> filter_func = [=](dfa_state_t left_in, dfa_state_t right_in)
  {
    if(left_in == left_sink)
      {
	return true;
      }

    return false;
  };

  std::vector<BitSet> pairs_by_layer;
  pairs_by_layer.reserve(ndim + 1);

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

  for(int layer = 0; layer < ndim; ++layer)
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

      populate_bitset<FilteredLayerTransitionsIterator<ndim, shape_pack...>>(next_layer,
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
	  pairs_by_layer.pop_back();
	  break;
	}
    }

  // apply leaf function

  profile.set_prefix("");
  profile.tic("leaves");

  std::vector<dfa_state_t> next_pair_mapping;
  if(pairs_by_layer.size() == ndim + 1)
    {
      BitSet& leaf_pairs = pairs_by_layer.at(ndim);
      assert(leaf_pairs.size() <= 4);

      for(int leaf_i = 0; leaf_i < 4; ++leaf_i)
	{
	  if(leaf_pairs.check(leaf_i))
	    {
	      size_t leaf_left_state = leaf_i / 2;
	      size_t leaf_right_state = leaf_i % 2;
	      next_pair_mapping.emplace_back(leaf_func(leaf_left_state, leaf_right_state));
	    }
	}
    }

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

      LayerTransitionsHelper<ndim, shape_pack...> helper(left_in, right_in, layer);

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

      auto compare_pair = [&](size_t curr_pair_a, size_t curr_pair_b)
      {
	for(int j = 0; j < curr_layer_shape; ++j)
	  {
	    dfa_state_t next_a = get_next_state(curr_pair_a, j);
	    dfa_state_t next_b = get_next_state(curr_pair_b, j);

	    if(next_a < next_b)
	      {
		return true;
	      }
	    else if(next_a > next_b)
	      {
		return false;
	      }
	  }

	return false;
      };

      profile.tic("backward sort");

      std::sort(curr_pairs.begin(), curr_pairs.end(), compare_pair);

      profile.tic("backward states");

      curr_pair_mapping.resize(curr_layer_count);

      // figure out first two states used
      dfa_state_t curr_logical = ~dfa_state_t(0);
      dfa_state_t next_logical = 2; // next state to be set

      auto set_helper = [&](size_t curr_pair)
      {
	DFATransitionsStaging set_transitions;
	for(int j = 0; j < curr_layer_shape; ++j)
	  {
	    set_transitions.push_back(get_next_state(curr_pair, j));
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

	this->set_state(layer, curr_logical, set_transitions);
      };

      set_helper(curr_pairs[0]);

      curr_pair_mapping[curr_index.rank(curr_pairs[0])] = curr_logical;

      for(size_t k = 1; k < curr_layer_count; ++k)
	{
	  if(compare_pair(curr_pairs[k-1], curr_pairs[k]))
	    {
	      set_helper(curr_pairs[k]);
	    }
	  curr_pair_mapping[curr_index.rank(curr_pairs[k])] = curr_logical;
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

// template instantiations

#include "DFAParams.h"

INSTANTIATE_DFA_TEMPLATE(BinaryDFA);
