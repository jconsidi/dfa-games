// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <assert.h>

#include <bit>
#include <iostream>
#include <queue>
#include <sstream>

#include "BitSet.h"
#include "Flashsort.h"
#include "MemoryMap.h"
#include "Profile.h"

template <int ndim, int... shape_pack>
class LayerTransitionsIterator;

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

  LayerTransitionsIterator<ndim, shape_pack...>  cend() const
  {
    return LayerTransitionsIterator<ndim, shape_pack...>(left, right, layer, layer_pairs.cend());
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

  size_t get_next_pair(size_t curr_i, int curr_j) const
  {
    assert(curr_i < curr_left_size * curr_right_size);
    dfa_state_t curr_left_state = curr_i / curr_right_size;
    dfa_state_t curr_right_state = curr_i % curr_right_size;

    const DFATransitions& left_transitions = left.get_transitions(layer, curr_left_state);
    const DFATransitions& right_transitions = right.get_transitions(layer, curr_right_state);

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
  BitSetIterator iter;

  mutable size_t curr_i;
  int curr_j;

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
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const DFA<ndim, shape_pack...>& left_in,
					  const DFA<ndim, shape_pack...>& right_in,
					  leaf_func_t leaf_func)
  : DedupedDFA<ndim, shape_pack...>()
{
  binary_build(left_in, right_in, leaf_func);
}

template <int ndim, int... shape_pack>
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>& dfas_in,
					  leaf_func_t leaf_func)
  : DedupedDFA<ndim, shape_pack...>()
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
	    for(dfa_state_t state_index = 0; state_index < layer_size; ++state_index)
	      {
		const DFATransitions& transitions(source->get_transitions(layer, state_index));
		dfa_state_t new_state_index = this->add_state(layer, transitions);
		assert(new_state_index == state_index);
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

template <int ndim, int... shape_pack>
void BinaryDFA<ndim, shape_pack...>::binary_build(const DFA<ndim, shape_pack...>& left_in,
						  const DFA<ndim, shape_pack...>& right_in,
						  leaf_func_t leaf_func)
{
  Profile profile("binary_build");

  std::vector<BitSet> pairs_by_layer;
  pairs_by_layer.reserve(ndim + 1);

  pairs_by_layer.emplace_back(left_in.get_layer_size(0) * right_in.get_layer_size(0));

  size_t initial_pair = left_in.get_initial_state() * right_in.get_layer_size(0) + right_in.get_initial_state();
  pairs_by_layer[0].prepare(initial_pair);
  pairs_by_layer[0].allocate();
  pairs_by_layer[0].add(initial_pair);

  // forward pass

  for(int layer = 0; layer < ndim; ++layer)
    {
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
	  std::ostringstream filename_builder;
	  filename_builder << "/tmp/chess/binarydfa-" << (layer < 9 ? "0" : "") << (layer + 1);
	  pairs_by_layer.emplace_back(filename_builder.str(), next_size);
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

      populate_bitset<LayerTransitionsIterator<ndim, shape_pack...>>(next_layer, layer_transitions.cbegin(), layer_transitions.cend());

      if(disk_mmap)
	{
	  size_t bits_set = next_layer.count();
	  size_t bits_total = next_layer.size();
	  std::cout << "bits set = " << bits_set << " / " << bits_total << " ~ " << (double(bits_set) / double(bits_total)) << std::endl;
	}
    }

  assert(pairs_by_layer.size() == ndim + 1);

  // apply leaf function

  profile.tic("leaves");

  BitSet& leaf_pairs = pairs_by_layer.at(ndim);
  assert(leaf_pairs.size() <= 4);

  std::vector<dfa_state_t> next_pair_mapping;
  for(int leaf_i = 0; leaf_i < 4; ++leaf_i)
    {
      if(leaf_pairs.check(leaf_i))
	{
	  size_t leaf_left_state = leaf_i / 2;
	  size_t leaf_right_state = leaf_i % 2;
	  next_pair_mapping.emplace_back(leaf_func(leaf_left_state, leaf_right_state));
	}
    }

  // backward pass

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      profile.tic("backward init");

      assert(next_pair_mapping.size() > 0);

      int curr_layer_shape = this->get_layer_shape(layer);
      const BitSet& curr_layer = pairs_by_layer.at(layer);
      std::vector<dfa_state_t> curr_pair_mapping;

      profile.tic("backward index");

      const BitSet& next_layer = pairs_by_layer.at(layer + 1);

      BitSetIndex next_index(next_layer);

      profile.tic("backward states");

      LayerTransitions layer_transitions(left_in, right_in, layer, curr_layer);

      DFATransitions next_transitions;
      for(auto iter = layer_transitions.cbegin();
	  iter < layer_transitions.cend();
	  ++iter)
	{
	  size_t next_i = *iter;
	  size_t next_logical = next_index.rank(next_i);
	  assert(next_logical < next_pair_mapping.size());
	  next_transitions.push_back(next_pair_mapping.at(next_logical));

	  if(next_transitions.size() == curr_layer_shape)
	    {
	      curr_pair_mapping.emplace_back(this->add_state(layer, next_transitions));
	      next_transitions.clear();
	    }
	}
      assert(next_transitions.size() == 0);
      next_pair_mapping.clear();
      std::swap(next_pair_mapping, curr_pair_mapping);

      assert(next_pair_mapping.size() > 0);
      assert(curr_pair_mapping.size() == 0);

      // cleanup in destructors
      profile.tic("backward cleanup");
    }

  assert(next_pair_mapping.size() == 1);
  this->set_initial_state(next_pair_mapping[0]);

  assert(this->ready());
  profile.tic("final cleanup");
}

// template instantiations

#include "DFAParams.h"

template class BinaryDFA<CHESS_DFA_PARAMS>;
template class BinaryDFA<TEST4_DFA_PARAMS>;
template class BinaryDFA<TEST5_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE2_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE3_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE4_DFA_PARAMS>;
