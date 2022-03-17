// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <assert.h>

#include <iostream>
#include <queue>
#include <sstream>

#include "MemoryMap.h"
#include "Profile.h"

struct CompareForwardChild
{
  bool operator()(const BinaryDFAForwardChild& a, const BinaryDFAForwardChild& b) const
    {
      return memcmp(&a, &b, sizeof(BinaryDFAForwardChild)) < 0;
    }
};

// flashsort in-situ permutation
// https://en.m.wikipedia.org/wiki/Flashsort
template<class T, class K>
void flashsort_permutation
(
 T *begin,
 T *end,
 std::function<K(const T&)> key_func
 )
{
  // LATER: tighten this up to reduce space usage. the original
  // Flashsort publication only uses one auxiliary array.

  // count occurrences of each key

  std::vector<K> key_counts;
  for(T *p = begin; p < end; ++p)
    {
      K key = key_func(*p);
      // extend counts as needed
      if(key_counts.size() <= key)
	{
	  key_counts.insert(key_counts.end(), key + 1 - key_counts.size(), 0);
	}
      assert(key < key_counts.size());

      key_counts[key] += 1;
    }

  // calculate boundaries between key ranges

  std::vector<T *> key_edges(key_counts.size() + 1);
  key_edges[0] = begin;
  for(K k = 0; k < key_counts.size(); ++k)
    {
      key_edges[k + 1] = key_edges[k] + key_counts[k];
    }
  assert(key_edges[key_counts.size()] == end);

  // in-situ permutation

  std::vector<T *> todo(key_counts.size());
  for(K k = 0; k < key_counts.size(); ++k)
    {
      todo[k] = key_edges[k];
    }

  for(K k = 0; k < key_counts.size(); ++k)
    {
      for(T *p = todo[k]; p < key_edges[k+1]; ++p)
	{
	  K temp_key = key_func(*p);
	  if(temp_key == k)
	    {
	      // already in right range
	      continue;
	    }

	  T temp = *p;
	  do
	    {
	      T *next = todo[temp_key]++;
	      std::swap(temp, *next);
	      temp_key = key_func(temp);
	    }
	  while(temp_key != k);

	  *p = temp;
       }
    }
}

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

  std::vector<MemoryMap<BinaryDFAForwardChild>> forward_children;
  forward_children.reserve(ndim);

  for(int layer = 0; layer < ndim; ++layer)
    {
      profile.tic("forward init");

      int layer_shape = this->get_layer_shape(layer);
      size_t forward_size = current_pairs.size();
      std::cout << "layer[" << layer << "] forward pairs = " << current_pairs.size() << std::endl;

      profile.tic("forward mmap");
      std::ostringstream filename_builder;
      filename_builder << "/tmp/chess-binarydfa-" << layer << "-children.bin";
      forward_children.emplace_back(filename_builder.str(), forward_size * layer_shape);
      MemoryMap<BinaryDFAForwardChild>& current_children = forward_children[layer];
      current_children.mmap();

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
	      forward_child.left = left_transitions[j];
	      forward_child.right = right_transitions[j];
	      forward_child.i = i;
	      forward_child.j = j;
	    }
	}
      std::cout << "layer[" << layer << "] forward children = " << current_children.size() << std::endl;

      profile.tic("forward permute right");
      size_t right_layer_size = (layer < ndim - 1) ? right_in.get_layer_size(layer+1) : 2;

      std::vector<dfa_state_t> right_to_dense(right_layer_size);
      std::vector<dfa_state_t> dense_to_right(right_layer_size);

      for(size_t l = 0; l < left_layer_size; ++l)
	{
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

      profile.tic("forward dedupe");

      current_pairs.clear();
      current_pairs.emplace_back(current_children[0].left, current_children[0].right);
      size_t children_size = current_children.size();
      for(size_t k = 1; k < children_size; ++k)
	{
	  if((current_children[k-1].left != current_children[k].left) ||
	     (current_children[k-1].right != current_children[k].right))
	    {
	      current_pairs.emplace_back(current_children[k].left, current_children[k].right);
	    }
	}

      std::cout << "layer[" << layer << "] next pairs = " << current_pairs.size() << std::endl;

      profile.tic("forward munmap");
      current_children.munmap();
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
      profile.tic("backward init");

      std::cout << "layer[" << layer << "] mapping children " << forward_children[layer].size() << std::endl;

      assert(backward_mapping.size() > 0);

      // apply previous mapping to previously sorted children

      profile.tic("backward mmap");
      MemoryMap<BinaryDFAForwardChild>& current_children = forward_children[layer];
      current_children.mmap();

      size_t mapped_size = current_children.size();
      std::vector<size_t> mapped_children;

      profile.tic("backward children");
      size_t mapped_index = 0;
      mapped_children.emplace_back(backward_mapping[mapped_index]);
      for(size_t k = 1; k < mapped_size; ++k)
	{
	  if((current_children[k].left != current_children[k-1].left) ||
	     (current_children[k].right != current_children[k-1].right))
	    {
	      mapped_index += 1;
	    }

	  mapped_children.emplace_back(backward_mapping[mapped_index]);
	}
      assert(mapped_index + 1 == backward_mapping.size());
      assert(mapped_children.size() == mapped_size);

      // reconstitute transitions

      profile.tic("backward transitions");

      std::vector<dfa_state_t> combined_transitions(mapped_size);

      int layer_shape = this->get_layer_shape(layer);
      for(size_t k = 0; k < mapped_size; ++k)
	{
	  size_t i = current_children[k].i;
	  size_t j = current_children[k].j;

	  combined_transitions.at(i * layer_shape + j) = mapped_children[k];
	}

      // done with memory map

      current_children.munmap();

      // add states for this layer

      profile.tic("backward states");

      backward_mapping.clear();
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
    }

  assert(this->ready());
}

// template instantiations

#include "ChessGame.h"
#include "TicTacToeGame.h"

template class BinaryDFA<CHESS_DFA_PARAMS>;
template class BinaryDFA<TEST_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE2_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE3_DFA_PARAMS>;
template class BinaryDFA<TICTACTOE4_DFA_PARAMS>;
