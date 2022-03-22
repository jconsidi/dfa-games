// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <assert.h>

#include <iostream>
#include <queue>

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

  std::vector<std::pair<dfa_state_t, dfa_state_t>> current_pairs;
  current_pairs.emplace_back(0, 0);

  // forward pass

  std::vector<MemoryMap<size_t>> forward_children_logical;
  forward_children_logical.reserve(ndim);

  for(int layer = 0; layer < ndim; ++layer)
    {
      std::cout << "layer[" << layer << "] forward" << std::endl;
      profile.tic("forward init");

      int layer_shape = this->get_layer_shape(layer);
      size_t forward_size = current_pairs.size();
      std::cout << "layer[" << layer << "] forward pairs = " << current_pairs.size() << std::endl;

      profile.tic("forward mmap");

      size_t current_children_size = forward_size * layer_shape;
      MemoryMap<BinaryDFAForwardChild> current_children =
	(current_children_size < size_t(1) << 31)
	? MemoryMap<BinaryDFAForwardChild>(current_children_size)
	: MemoryMap<BinaryDFAForwardChild>("/tmp/chess-binarydfa-index.bin", current_children_size);

      profile.tic("forward left counts");

      size_t left_layer_size = (layer < ndim - 1) ? left_in.get_layer_size(layer+1) : 2;
      std::vector<size_t> left_counts(left_layer_size);
      assert(left_counts[0] == 0);
      assert(left_counts[left_counts.size() - 1] == 0);

      for(size_t i = 0; i < forward_size; ++i)
	{
	  const std::pair<dfa_state_t, dfa_state_t>& forward_pair = current_pairs[i];

	  const DFATransitions& left_transitions = left_in.get_transitions(layer, std::get<0>(forward_pair));

	  for(int j = 0; j < layer_shape; ++j)
	    {
	      left_counts[left_transitions[j]] += 1;
	    }
	}

      std::vector<size_t> left_edges(left_layer_size + 1);
      left_edges[0] = 0;
      for(size_t k = 0; k < left_layer_size; ++k)
	{
	  left_edges[k+1] = left_edges[k] + left_counts[k];
	}
      assert(left_edges[left_layer_size] == current_children.size());

      std::vector<size_t> left_todo(left_layer_size);
      for(size_t k = 0; k < left_layer_size; ++k)
	{
	  left_todo[k] = left_edges[k];
	}

      profile.tic("forward children left");

      // build forward children already sorted by left child.
      // basically combining counting sort with the creation.

      for(size_t i = 0; i < forward_size; ++i)
	{
	  const std::pair<dfa_state_t, dfa_state_t>& forward_pair = current_pairs[i];

	  const DFATransitions& left_transitions = left_in.get_transitions(layer, std::get<0>(forward_pair));
	  const DFATransitions& right_transitions = right_in.get_transitions(layer, std::get<1>(forward_pair));

	  for(int j = 0; j < layer_shape; ++j)
	    {
	      auto left = left_transitions[j];
	      BinaryDFAForwardChild& forward_child = current_children[left_todo[left]++];
	      forward_child.right = right_transitions[j];
	      forward_child.i = i;
	      forward_child.j = j;
	    }
	}
      std::cout << "layer[" << layer << "] forward children = " << current_children.size() << std::endl;

      profile.tic("forward permute right");
      size_t right_layer_size = (layer < ndim - 1) ? right_in.get_layer_size(layer+1) : 2;

      double right_factor = double(left_layer_size) * double(right_layer_size) / double(current_children.size());
      std::cout << "layer[" << layer << "] right factor = " << right_factor << std::endl;

      std::vector<dfa_state_t> right_to_dense(right_layer_size);
      std::vector<dfa_state_t> dense_to_right(right_layer_size);

      for(size_t l = 0; l < left_layer_size; ++l)
	{
	  size_t left_count = left_edges[l + 1] - left_edges[l];
	  if(left_count == 0)
	    {
	      continue;
	    }

	  if(left_count < right_layer_size / 2)
	    {
	      // sparse case
	      std::cout << "sparse " << left_count << " vs " << right_layer_size << std::endl;

	      // map to dense mapping to run linear in left_count
	      // instead of linear in right_layer_size.

	      dfa_state_t dense_size = 0;

	      // map right values to dense ids, and count the dense ids
	      for(size_t k = left_edges[l]; k < left_edges[l+1]; ++k)
		{
		  dfa_state_t right = current_children[k].right;
		  dfa_state_t dense_id = right_to_dense[right];
		  if((dense_id >= dense_size) || (dense_to_right[dense_id] != right))
		    {
		      // new right value
		      dense_id = right_to_dense[right] = dense_size++;
		      dense_to_right[dense_id] = right;
		    }
		}

	      // sort by dense mapping of right children. will not be
	      // sorted overall, but matching left/right pairs will be
	      // together.

	      flashsort_permutation<BinaryDFAForwardChild, dfa_state_t>
		(
		 current_children.begin() + left_edges[l],
		 current_children.begin() + left_edges[l+1],
		 [&](const BinaryDFAForwardChild& v)
		 {
		   return right_to_dense.at(v.right);
		 }
		 );
	    }
	  else
	    {
	      // dense case
	      std::cout << "dense " << left_count << " vs " << right_layer_size << std::endl;

	      flashsort_permutation<BinaryDFAForwardChild, dfa_state_t>
		(
		 current_children.begin() + left_edges[l],
		 current_children.begin() + left_edges[l+1],
		 [&](const BinaryDFAForwardChild& v)
		 {
		   return v.right;
		 }
		 );
	    }
	}

      profile.tic("forward logical mmap");

      forward_children_logical.emplace_back(forward_size * layer_shape);
      MemoryMap<size_t>& current_children_logical = forward_children_logical.at(layer);

      profile.tic("forward dedupe and logical");

      current_pairs.clear();
      size_t current_logical = 0;
      for(size_t l = 0; l < left_layer_size; ++l)
	{
	  if(left_edges[l] == left_edges[l+1])
	    {
	      // no children with this left state
	      continue;
	    }

	  const BinaryDFAForwardChild& first_child = current_children[left_edges[l]];
	  current_pairs.emplace_back(l, first_child.right);
	  current_children_logical[first_child.i * layer_shape + first_child.j] = current_logical;

	  for(size_t k = left_edges[l] + 1; k < left_edges[l+1]; ++k)
	    {
	      const BinaryDFAForwardChild& current_child = current_children[k];
	      if(current_children[k-1].right != current_child.right)
		{
		  current_pairs.emplace_back(l, current_child.right);
		  ++current_logical;
		}
	      current_children_logical[current_child.i * layer_shape + current_child.j] = current_logical;
	    }

	  // done with this left value
	  ++current_logical;
	}
      assert(current_logical == current_pairs.size());

      std::cout << "layer[" << layer << "] next pairs = " << current_pairs.size() << std::endl;

      // cleanup in destructors
      profile.tic("forward cleanup");
    }

  // apply leaf function

  profile.tic("forward leaves");

  std::vector<size_t> backward_mapping;

  for(size_t i = 0; i < current_pairs.size(); ++i)
    {
      const std::pair<dfa_state_t, dfa_state_t>& leaf_pair = current_pairs[i];
      backward_mapping.emplace_back(leaf_func(std::get<0>(leaf_pair), std::get<1>(leaf_pair)));
    }

  // backward pass

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      std::cout << "layer[" << layer << "] backward" << std::endl;
      profile.tic("backward init");

      assert(backward_mapping.size() > 0);

      // apply previous mapping to previously sorted children

      MemoryMap<size_t>& current_children_logical = forward_children_logical[layer];

      // reconstitute transitions

      profile.tic("backward transitions");

      size_t mapped_size = current_children_logical.size();
      std::vector<dfa_state_t> combined_transitions(mapped_size);

      for(size_t k = 0; k < mapped_size; ++k)
	{
	  combined_transitions[k] = backward_mapping.at(current_children_logical[k]);
	}

      // add states for this layer

      profile.tic("backward states");

      backward_mapping.clear();
      int layer_shape = this->get_layer_shape(layer);
      size_t output_size = mapped_size / layer_shape;
      for(size_t i = 0; i < output_size; ++i)
	{
	  DFATransitions transitions(layer_shape);
	  for(int j = 0; j < layer_shape; ++j)
	    {
	      transitions[j] = combined_transitions.at(i * layer_shape + j);
	    }
	  backward_mapping.push_back(this->add_state(layer, transitions));
	}

      // cleanup in destructors
      profile.tic("backward cleanup");
    }

  assert(this->ready());
  profile.tic("final cleanup");
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class BinaryDFA<CHESS_DFA_PARAMS>;
template class BinaryDFA<TEST_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE2_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE3_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE4_DFA_PARAMS>;
