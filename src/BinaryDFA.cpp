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
	    int layer_size = source->get_layer_size(layer);
	    for(int state_index = 0; state_index < layer_size; ++state_index)
	      {
		const DFATransitions& transitions(source->get_transitions(layer, state_index));
		int new_state_index = this->add_state(layer, transitions);
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

  std::vector<std::pair<int, int>> current_pairs;
  current_pairs.emplace_back(0, 0);

  // forward pass

  std::vector<MemoryMap<BinaryDFAForwardChild>> forward_children;
  forward_children.reserve(ndim);

  for(int layer = 0; layer < ndim; ++layer)
    {
      profile.tic("forward init");

      int layer_shape = this->get_layer_shape(layer);
      int forward_size = current_pairs.size();
      std::cout << "layer[" << layer << "] forward pairs = " << current_pairs.size() << std::endl;

      profile.tic("forward mmap");
      std::ostringstream filename_builder;
      filename_builder << "/tmp/chess-binarydfa-" << layer << "-children.bin";
      forward_children.emplace_back(filename_builder.str(), forward_size * layer_shape);
      MemoryMap<BinaryDFAForwardChild>& current_children = forward_children[layer];
      current_children.mmap();

      profile.tic("forward children");
      int left_layer_size = (layer < ndim - 1) ? left_in.get_layer_size(layer+1) : 2;
      std::vector<int> left_counts(left_layer_size);
      assert(left_counts[0] == 0);
      assert(left_counts[left_counts.size() - 1] == 0);
      for(int i = 0; i < forward_size; ++i)
	{
	  const std::pair<int, int>& forward_pair = current_pairs[i];

	  const DFATransitions& left_transitions = left_in.get_transitions(layer, std::get<0>(forward_pair));
	  const DFATransitions& right_transitions = right_in.get_transitions(layer, std::get<1>(forward_pair));

	  for(int j = 0; j < layer_shape; ++j)
	    {
	      BinaryDFAForwardChild& forward_child = current_children[i * layer_shape + j];
	      forward_child.left = left_transitions[j];
	      forward_child.right = right_transitions[j];
	      forward_child.i = i;
	      forward_child.j = j;

	      left_counts[forward_child.left] += 1;
	      assert(left_counts[forward_child.left]);
	    }
	}
      std::cout << "layer[" << layer << "] forward children = " << current_children.size() << std::endl;

      // linear time permutations to sort by left child based on
      // Flashsort. http://www.ddj.com/architect/184410496

      profile.tic("forward permute left");
      std::cout << "layer[" << layer << "] left layer size = " << left_layer_size << std::endl;

      std::vector<int> left_edges(left_layer_size + 1);
      left_edges[0] = 0;
      for(int k = 0; k < left_layer_size; ++k)
	{
	  left_edges[k+1] = left_edges[k] + left_counts[k];
	}
      assert(left_edges[left_layer_size] == current_children.size());

      std::vector<int> left_todo(left_layer_size);
      for(int k = 0; k < left_layer_size; ++k)
	{
	  left_todo[k] = left_edges[k];
	}

      for(int k = 0; k < left_layer_size; ++k)
	{
	  for(int i = left_todo[k]; i < left_edges[k+1]; ++i)
	    {
	      if(current_children[i].left == k)
		{
		  // already in right range
		  continue;
		}

	      BinaryDFAForwardChild temp = current_children[i];
	      while(temp.left != k)
		{
		  int next_index = left_todo[temp.left]++;
		  std::swap(temp, current_children[next_index]);
		}
	      current_children[i] = temp;
	    }
	}

      for(int k = 0; k < left_layer_size; ++k)
	{
	  for(int i = left_edges[k]; i < left_edges[k+1]; ++i)
	    {
	      assert(current_children[i].left == k);
	    }
	}

      profile.tic("forward permute right");
      int right_layer_size = (layer < ndim - 1) ? right_in.get_layer_size(layer+1) : 2;
      std::cout << "layer[" << layer << "] right layer size = " << right_layer_size << std::endl;

      for(int k = 0; k < left_layer_size; ++k)
	{
	  std::sort(current_children.begin() + left_edges[k], current_children.begin() + left_edges[k+1], CompareForwardChild());
	}

      profile.tic("forward dedupe");

      current_pairs.clear();
      current_pairs.emplace_back(current_children[0].left, current_children[0].right);
      int children_size = current_children.size();
      for(int k = 1; k < children_size; ++k)
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

  std::vector<int> backward_mapping[ndim+1];
  std::vector<int>& leaf_mapping = backward_mapping[ndim];

  for(int i = 0; i < current_pairs.size(); ++i)
    {
      const std::pair<int, int>& leaf_pair = current_pairs[i];
      leaf_mapping.emplace_back(leaf_func(std::get<0>(leaf_pair), std::get<1>(leaf_pair)));
    }

  // backward pass

  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      profile.tic("backward init");

      std::cout << "layer[" << layer << "] mapping children " << forward_children[layer].size() << std::endl;

      assert(backward_mapping[layer+1].size() > 0);
      assert(backward_mapping[layer].size() == 0);

      // apply previous mapping to previously sorted children

      profile.tic("backward mmap");
      MemoryMap<BinaryDFAForwardChild>& current_children = forward_children[layer];
      current_children.mmap();

      int mapped_size = current_children.size();
      std::vector<int> mapped_children;

      profile.tic("backward children");
      std::vector<int>& mapped_pairs = backward_mapping[layer+1];
      int mapped_index = 0;
      mapped_children.emplace_back(mapped_pairs[mapped_index]);
      for(int k = 1; k < mapped_size; ++k)
	{
	  if((current_children[k].left != current_children[k-1].left) ||
	     (current_children[k].right != current_children[k-1].right))
	    {
	      mapped_index += 1;
	    }

	  mapped_children.emplace_back(mapped_pairs[mapped_index]);
	}
      assert(mapped_index + 1 == mapped_pairs.size());
      assert(mapped_children.size() == mapped_size);

      // reconstitute transitions

      profile.tic("backward transitions");

      std::vector<int> combined_transitions(mapped_size);

      int layer_shape = this->get_layer_shape(layer);
      for(int k = 0; k < mapped_size; ++k)
	{
	  int i = current_children[k].i;
	  int j = current_children[k].j;

	  combined_transitions.at(i * layer_shape + j) = mapped_children[k];
	}

      // done with memory map

      current_children.munmap();

      // add states for this layer

      profile.tic("backward states");

      std::vector<int>& output_mapping = backward_mapping[layer];
      int output_size = mapped_size / layer_shape;
      for(int i = 0; i < output_size; ++i)
	{
	  DFATransitions transitions(layer_shape);
	  for(int j = 0; j < layer_shape; ++j)
	    {
	      transitions[j] = combined_transitions.at(i * layer_shape + j);
	    }
	  output_mapping.push_back(this->add_state(layer, transitions));
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
