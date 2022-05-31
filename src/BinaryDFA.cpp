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
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const DFA<ndim, shape_pack...>& left_in,
					  const DFA<ndim, shape_pack...>& right_in,
					  leaf_func_t leaf_func)
  : DFA<ndim, shape_pack...>()
{
  binary_build(left_in, right_in, leaf_func);
}

template <int ndim, int... shape_pack>
BinaryDFA<ndim, shape_pack...>::BinaryDFA(const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>& dfas_in,
					  leaf_func_t leaf_func)
  : DFA<ndim, shape_pack...>()
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
  pairs_by_layer[0].add(left_in.get_initial_state() * right_in.get_layer_size(0) + right_in.get_initial_state());

  // forward pass

  for(int layer = 0; layer < ndim; ++layer)
    {
      profile.tic("forward init");

      int curr_layer_shape = this->get_layer_shape(layer);

      // get size of current layer and next layer.
      //
      // using size_t for product calculations...

      size_t curr_left_size = left_in.get_layer_size(layer);
      size_t curr_right_size = right_in.get_layer_size(layer);

      size_t next_left_size = left_in.get_layer_size(layer + 1);
      size_t next_right_size = right_in.get_layer_size(layer + 1);

      profile.tic("forward mmap");

      assert(pairs_by_layer.size() == layer + 1);
      size_t next_size = next_left_size * next_right_size;
      bool disk_mmap = next_size >= 1ULL << 32;
      if(disk_mmap)
	{
	  std::ostringstream filename_builder;
	  filename_builder << "/tmp/chess-binarydfa-" << (layer + 1) << "-children.bin";
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

      for(auto iter = curr_layer.cbegin();
	  iter < curr_layer.cend();
	  ++iter)
	{
	  size_t curr_i = *iter;
	  assert(curr_i < curr_left_size * curr_right_size);
	  dfa_state_t curr_left_state = curr_i / curr_right_size;
	  dfa_state_t curr_right_state = curr_i % curr_right_size;

	  const DFATransitions& left_transitions = left_in.get_transitions(layer, curr_left_state);
	  const DFATransitions& right_transitions = right_in.get_transitions(layer, curr_right_state);

	  for(int curr_j = 0; curr_j < curr_layer_shape; ++curr_j)
	    {
	      size_t next_left_state = left_transitions.at(curr_j);
	      assert(next_left_state < next_left_size);
	      size_t next_right_state = right_transitions.at(curr_j);
	      assert(next_right_state < next_right_size);
	      size_t next_i = next_left_state * next_right_size + next_right_state;
	      next_layer.add(next_i);
	    }
	}

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

      const BitSet& curr_layer = pairs_by_layer.at(layer);
      std::vector<dfa_state_t> curr_pair_mapping;

      size_t curr_left_size = left_in.get_layer_size(layer);
      size_t curr_right_size = right_in.get_layer_size(layer);

      profile.tic("backward index");

      size_t next_left_size = left_in.get_layer_size(layer + 1);
      size_t next_right_size = right_in.get_layer_size(layer + 1);
      const BitSet& next_layer = pairs_by_layer.at(layer + 1);

      BitSetIndex next_index(next_layer);

      profile.tic("backward states");

      for(auto iter = curr_layer.cbegin();
	  iter < curr_layer.cend();
	  ++iter)
	{
	  size_t curr_i = *iter;
	  dfa_state_t curr_left_state = curr_i / curr_right_size;
	  dfa_state_t curr_right_state = curr_i % curr_right_size;

	  assert(curr_left_state < curr_left_size);

	  const DFATransitions& left_transitions = left_in.get_transitions(layer, curr_left_state);
	  const DFATransitions& right_transitions = right_in.get_transitions(layer, curr_right_state);

	  curr_pair_mapping.emplace_back(this->add_state(layer, [&](int j)
	  {
	    size_t next_left_state = left_transitions.at(j);
	    assert(next_left_state < next_left_size);
	    size_t next_right_state = right_transitions.at(j);
	    assert(next_right_state < next_right_size);
	    size_t next_i = next_left_state * next_right_size + next_right_state;
	    assert(next_i < next_left_size * next_right_size);

	    size_t next_logical = next_index[next_i];
	    assert(next_logical < next_pair_mapping.size());
	    return next_pair_mapping.at(next_logical);
	  }));
	}
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

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class BinaryDFA<CHESS_DFA_PARAMS>;
template class BinaryDFA<TEST4_DFA_PARAMS>;
template class BinaryDFA<TEST5_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE2_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE3_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE4_DFA_PARAMS>;
