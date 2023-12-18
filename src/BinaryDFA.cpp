// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <execution>
#include <iostream>
#include <memory>
#include <queue>
#include <ranges>
#include <sstream>
#include <string>
#include <unistd.h>

#include "Flashsort.h"
#include "MemoryMap.h"
#include "Profile.h"
#include "VectorBitSet.h"
#include "parallel.h"
#include "sort.h"

static const size_t SYNC_THRESHOLD_BYTES = 1ULL << 25;

static void sync_if_big(size_t length)
{
  // sync to disk if more than 32MB

  if(length >= SYNC_THRESHOLD_BYTES)
    {
      sync();
    }
}

template<class T>
static void sync_if_big(MemoryMap<T>& memory_map)
{
  if(memory_map.length() >= SYNC_THRESHOLD_BYTES)
    {
      memory_map.msync();
    }
}

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
  filename_builder << "scratch/binarydfa/layer=" << (layer < 10 ? "0" : "") << layer;
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

template<class T>
static MemoryMap<T> memory_map_helper(int layer, std::string suffix, size_t size_in)
{
  return MemoryMap<T>(binary_build_file_prefix(layer) + "-" + suffix, size_in);
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

  dfa_state_t initial_left = left_in.get_initial_state();
  dfa_state_t initial_right = right_in.get_initial_state();
  if(filter_func(initial_left, initial_right))
    {
      this->set_initial_state(shortcircuit_func(initial_left, initial_right));
      return;
    }

  // all reachable pairs of left/right states, represented as a size_t
  // instead of a pair of dfa_state_t's.

  std::vector<MemoryMap<size_t>> pairs_by_layer;
  pairs_by_layer.reserve(get_shape_size() + 1);

  // manual setup of initial layer

  size_t initial_pair = initial_left * right_in.get_layer_size(0) + initial_right;
  pairs_by_layer.push_back(memory_map_helper<size_t>(0, "pairs", 1));
  pairs_by_layer[0][0] = initial_pair;

  // transitions helper

  std::function<MemoryMap<size_t>(int)> build_transition_pairs = [&](int layer)
  {
    Profile profile2("build_transition_pairs");

    profile2.tic("init");

    int curr_layer_shape = this->get_layer_shape(layer);
    const MemoryMap<size_t>& curr_pairs = pairs_by_layer.at(layer);
    assert(curr_pairs.size() > 0);
    curr_pairs.mmap();

    size_t curr_left_size = left_in.get_layer_size(layer);
    size_t curr_right_size = right_in.get_layer_size(layer);
    assert(curr_pairs[curr_pairs.size() - 1] < curr_left_size * curr_right_size);
    size_t next_right_size = right_in.get_layer_size(layer + 1);

    MemoryMap<size_t> curr_transition_pairs("scratch/binarydfa/transition_pairs",
					    curr_pairs.size() * curr_layer_shape);

    // make sure inputs are memory mapped before going parallel
    curr_pairs.mmap();
    left_in.get_transitions(layer, 0);
    right_in.get_transitions(layer, 0);

    // read left transitions
    profile2.tic("left");

    auto update_left_transition = [&](const size_t& curr_pair)
    {
      size_t curr_i = &curr_pair - curr_pairs.begin();
      assert(curr_pairs[curr_i] == curr_pair);

      size_t curr_pair_offset = curr_i * curr_layer_shape;

      size_t curr_left_state = curr_pair / curr_right_size;
      assert(curr_left_state < curr_left_size);
      DFATransitionsReference left_transitions = left_in.get_transitions(layer, curr_left_state);

      for(int curr_j = 0; curr_j < curr_layer_shape; ++curr_j)
	{
	  dfa_state_t next_left_state = left_transitions.at(curr_j);
	  curr_transition_pairs[curr_pair_offset + curr_j] = size_t(next_left_state) * next_right_size;
	}
    };

#ifdef __cpp_lib_parallel_algorithm
    // do first update serially to avoid race condition on left DFA mmap
    update_left_transition(curr_pairs[0]);

    std::for_each(std::execution::par_unseq,
		  curr_pairs.begin() + 1,
		  curr_pairs.end(),
		  update_left_transition);
#else
    for(size_t curr_i = 0; curr_i < curr_pairs.size(); ++curr_i)
      {
	update_left_transition(curr_pairs[curr_i]);
      }
#endif

    // read right transitions
    profile2.tic("right");

    auto update_right_transition = [&](const size_t& curr_pair)
    {
      size_t curr_i = &curr_pair - curr_pairs.begin();
      assert(curr_pairs[curr_i] == curr_pair);

      size_t curr_pair_offset = curr_i * curr_layer_shape;

      size_t curr_right_state = curr_pair % curr_right_size;
      DFATransitionsReference right_transitions = right_in.get_transitions(layer, curr_right_state);

      for(int curr_j = 0; curr_j < curr_layer_shape; ++curr_j)
	{
	  dfa_state_t next_right_state = right_transitions.at(curr_j);
	  curr_transition_pairs[curr_pair_offset + curr_j] += size_t(next_right_state);
	}
    };

#ifdef __cpp_lib_parallel_algorithm
    // do first update serially to avoid race condition on right DFA mmap
    update_right_transition(curr_pairs[0]);

    std::for_each(std::execution::par_unseq,
		  curr_pairs.begin() + 1,
		  curr_pairs.end(),
		  update_right_transition);
#else
    for(size_t curr_i = 0; curr_i < curr_pairs.size(); ++curr_i)
      {
	update_right_transition(curr_pairs[curr_i]);
      }
#endif

    // flush to disk

    profile2.tic("sync");

    sync_if_big<size_t>(curr_transition_pairs);

    // cleanup

    profile2.tic("curr pairs munmap");

    curr_pairs.munmap();

    // done
    profile2.tic("done");
    return curr_transition_pairs;
  };

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

      assert(pairs_by_layer.size() == layer + 1);

      profile.tic("forward transition pairs");

      MemoryMap<size_t> curr_transition_pairs = build_transition_pairs(layer);

      std::cout << "pair count = " << curr_transition_pairs.size() << " (original)" << std::endl;

      profile.tic("forward transition pairs sync");

      sync_if_big<size_t>(curr_transition_pairs);

      // helper functions

      auto copy_helper = [&](size_t *begin, size_t *end, size_t *dest)
      {
	assert(dest <= begin);
	assert(begin < end);

	ssize_t source_count = end - begin;
	ssize_t no_overlap_count = begin - dest;
	if(source_count <= no_overlap_count)
	  {
	    // no overlap -> parallel copy
	    dest = TRY_PARALLEL_3(std::copy, begin, end, dest);
	  }
	else
	  {
	    // overlap -> serial copy
	    dest = std::copy(begin, end, dest);
	  }

	sync_if_big((end - begin) * sizeof(size_t));

	return dest;
      };

      profile.tic("forward transition pairs filter unique");

      auto remove_func = [&](size_t next_pair)
      {
	  dfa_state_t next_left_state = next_pair / next_right_size;
	  dfa_state_t next_right_state = next_pair % next_right_size;

	  return filter_func(next_left_state, next_right_state);
      };

      size_t filter_block_size = 1ULL << 25;

      size_t *filter_end = curr_transition_pairs.begin();
      for(size_t *block_begin = curr_transition_pairs.begin();
	  block_begin < curr_transition_pairs.end();
	  block_begin += filter_block_size)
	{
	  size_t *block_end = std::min(block_begin + filter_block_size,
				       curr_transition_pairs.end());
	  size_t block_bytes = (block_end - block_begin) * sizeof(size_t);

	  block_end = TRY_PARALLEL_3(std::remove_if, block_begin, block_end, remove_func);
	  sync_if_big(block_bytes);
	  if(block_end == block_begin)
	    {
	      continue;
	    }

	  block_end = TRY_PARALLEL_2(std::unique, block_begin, block_end);
	  sync_if_big(block_bytes);

	  filter_end = copy_helper(block_begin, block_end, filter_end);
	}

      std::cout << "pair count = " << (filter_end - curr_transition_pairs.begin()) << " (post filter unique)" << std::endl;

      profile.tic("forward transition pairs sort unique");

      size_t *unique_end = curr_transition_pairs.begin();

      auto check_next_pair = [&](size_t next_pair)
      {
	if(unique_end > curr_transition_pairs.begin())
	  {
	    assert(*(unique_end - 1) < next_pair);
	  }
      };

      std::vector<std::tuple<size_t *, size_t *, size_t, size_t>> unique_queue;
      unique_queue.emplace_back(curr_transition_pairs.begin(),
				filter_end,
				0,
				next_left_size * next_right_size);
      while(unique_queue.size())
	{
	  Profile profile2("unique");
	  profile2.tic("init");

	  auto unique_todo = unique_queue.back();
	  unique_queue.pop_back();

	  // moving [begin, end) which has values in [value_min, value_max)
	  // to [unique_end, ...) after filtering, sorting, and deduping.
	  size_t *begin = std::get<0>(unique_todo);
	  size_t *end = std::get<1>(unique_todo);
	  size_t value_min = std::get<2>(unique_todo);
	  size_t value_max = std::get<3>(unique_todo);

	  assert(unique_end <= begin);
	  assert(begin <= end);
	  assert(value_min < value_max);
	  check_next_pair(value_min);

	  // constant syncs to ward off the OOM killer
	  size_t range_bytes = (end - begin) * sizeof(size_t);
	  sync_if_big(range_bytes);

	  if(begin == end)
	    {
	      // empty range
	      continue;
	    }

	  size_t value_width = value_max - value_min;
	  if(value_width <= 1024)
	    {
	      // small range of values remaining, so go with count
	      // sort approach and make a bitmap of which ones are
	      // present.

	      profile2.tic("count");

	      std::vector<bool> values_present(value_width, false);
	      for(size_t *v_iter = begin; v_iter < end; ++v_iter)
		{
		  assert(value_min <= *v_iter);
		  assert(*v_iter < value_max);
		  values_present.at(*v_iter - value_min) = true;
		}

	      for(size_t v = value_min; v < value_max; ++v)
		{
		  if(values_present.at(v - value_min))
		    {
		      *unique_end++ = v;
		    }
		}

	      continue;
	    }

	  if(range_bytes <= 1ULL << 28)
	    {
	      // "small" range, so just handle directly

	      profile2.tic("base");

	      TRY_PARALLEL_2(std::sort, begin, end);
	      sync_if_big(range_bytes);

	      end = TRY_PARALLEL_2(std::unique, begin, end);
	      sync_if_big(range_bytes);

	      check_next_pair(*begin);
	      unique_end = copy_helper(begin, end, unique_end);
	      sync_if_big(range_bytes);

	      continue;
	    }

	  // use flashsort permutation to partition by value

	  profile2.tic("partition");

	  size_t target_buckets = 1024;
	  size_t divisor = (value_width + target_buckets - 1) / target_buckets;
	  assert(divisor > 0);

	  std::vector<size_t *> partition = flashsort_partition<size_t, size_t>(begin, end, [=](const size_t& v){return (v - value_min) / divisor;});
	  assert(partition.at(0) == begin);
	  assert(partition.back() == end);
	  assert(partition.size() - 1 <= target_buckets);
	  sync_if_big(range_bytes);

	  for(int i = partition.size() - 2; i >= 0; --i)
	    {
	      assert(partition[i] <= partition[i + 1]);
	      if(partition[i] == partition[i + 1])
		{
		  continue;
		}

	      size_t partition_value_min = value_min + i * divisor;
	      size_t partition_value_max = value_min + (i + 1) * divisor;

#ifdef PARANOIA
	      size_t partition_value_min_check = *std::min_element(partition[i], partition[i+1]);
	      size_t partition_value_max_check = *std::max_element(partition[i], partition[i+1]);
	      assert(partition_value_min <= partition_value_min_check);
	      assert(partition_value_max_check < partition_value_max);
#endif

	      unique_queue.emplace_back(partition[i],
					partition[i + 1],
					partition_value_min,
					partition_value_max);
	    }
	}

      std::cout << "pair count = " << (unique_end - curr_transition_pairs.begin()) << " (post sort unique)" << std::endl;

      profile.tic("forward transitions msync");

      sync_if_big<size_t>(curr_transition_pairs);

      profile.tic("forward next pairs count");

      size_t next_pairs_count = unique_end - curr_transition_pairs.begin();

      profile.tic("forward next pairs mmap");

      pairs_by_layer.emplace_back(memory_map_helper<size_t>(layer + 1, "pairs", next_pairs_count));
      MemoryMap<size_t>& next_pairs = pairs_by_layer.at(layer + 1);

      if(next_pairs_count == 0)
	{
	  break;
	}

      profile.tic("forward next pairs populate");

      auto copy_end = std::copy(
#ifdef __cpp_lib_parallel_algorithm
				   std::execution::par_unseq,
#endif
				   curr_transition_pairs.begin(),
				   unique_end,
				   next_pairs.begin());
      assert(copy_end == next_pairs.end());

      profile.tic("forward next pairs msync");

      sync_if_big<size_t>(next_pairs);

      profile.tic("forward next pairs paranoia");

      assert(TRY_PARALLEL_2(std::is_sorted, next_pairs.begin(), next_pairs.end()));

      profile.tic("forward stats");

      if(next_pairs.size() >= 100000)
	{
	  // stats compared to full quadratic blow up
	  size_t bits_set = next_pairs.size();
	  size_t bits_total = next_left_size * next_right_size;
	  std::cout << "bits set = " << bits_set << " / " << bits_total << " ~ " << (double(bits_set) / double(bits_total)) << std::endl;
	}

      if(next_pairs.size() == 0)
	{
	  // all pairs in next layer were filtered. no need to continue.
	  break;
	}

      profile.tic("forward cleanup");
    }

  assert(pairs_by_layer.size() > 0);
  assert(pairs_by_layer.back().size() == 0);

  // apply leaf function

  profile.set_prefix("");
  profile.tic("leaves");

  // initially empty since shortcircuiting will happen by last layer.
  MemoryMap<dfa_state_t> next_pair_rank_to_output(1); // dummy initialization

  // backward pass

  for(int layer = pairs_by_layer.size() - 2; layer >= 0; --layer)
    {
      profile.set_prefix("layer=" + std::to_string(layer));
      profile.tic("backward init");

      assert(pairs_by_layer.size() == layer + 2);

      int curr_layer_shape = this->get_layer_shape(layer);
      const MemoryMap<size_t>& curr_pairs = pairs_by_layer.at(layer);
      size_t curr_layer_count = curr_pairs.size();

      size_t next_left_size = left_in.get_layer_size(layer + 1);
      size_t next_right_size = right_in.get_layer_size(layer + 1);

      profile.tic("backward next pair mmap");

      const MemoryMap<size_t>& next_pairs = pairs_by_layer.at(layer+1);
      next_pairs.mmap();
      size_t next_layer_count = next_pairs.size();

      profile.tic("backward next pair index");

      // index entries have first pair of 4KB block (64 bit pair)
      std::vector<MemoryMap<size_t>> next_pairs_index;
      next_pairs_index.reserve(3);
      auto add_next_pairs_index = [&](const MemoryMap<size_t>& previous_pairs)
      {
	std::string index_name = "scratch/binarydfa/next_pairs_index-" + std::to_string(next_pairs_index.size());
	size_t index_length = (previous_pairs.size() + 511) / 512;
	next_pairs_index.emplace_back(index_name, index_length);
	MemoryMap<size_t>& new_index = next_pairs_index.back();

	for(size_t i = 0; i < new_index.size(); ++i)
	  {
	    new_index[i] = previous_pairs[i * 512];
	  }

	sync_if_big(new_index);
      };
      add_next_pairs_index(next_pairs);
      assert(next_pairs_index.size() == 1);

      // add more indexes until under 1MB
      while(next_pairs_index.back().length() > 1ULL << 20)
	{
	  assert(next_pairs_index.size() < next_pairs_index.capacity());
	  add_next_pairs_index(next_pairs_index.back());
	}

      auto search_index = [&](const MemoryMap<size_t>& next_pairs_index, size_t next_pair, size_t offset_min, size_t offset_max)
      {
	assert(offset_min <= offset_max);
	assert(offset_max < next_pairs_index.size());

	while(offset_min < offset_max)
	  {
	    size_t offset_mid = offset_min + (offset_max - offset_min + 1) / 2;
	    if(next_pairs_index[offset_mid] <= next_pair)
	      {
		offset_min = offset_mid;
	      }
	    else
	      {
		offset_max = offset_mid - 1;
	      }
	  }

	return offset_min;
      };

      profile.tic("backward transitions input");

      MemoryMap<size_t> curr_transition_pairs = build_transition_pairs(layer);

      profile.tic("backward transitions mmap");

      MemoryMap<dfa_state_t> curr_transitions("scratch/binarydfa/transitions", curr_layer_count * curr_layer_shape);

      profile.tic("backward transitions populate");

      auto calculate_backward_transition = [&](size_t next_pair)
      {
	dfa_state_t next_left_state = next_pair / next_right_size;
	assert(next_left_state < next_left_size);
	dfa_state_t next_right_state = next_pair % next_right_size;

	if(filter_func(next_left_state, next_right_state))
	  {
	    return shortcircuit_func(next_left_state, next_right_state);
	  }
	else
	  {
	    size_t offset_min = 0;
	    size_t offset_max = next_pairs_index.back().size() - 1;

	    for(int index_index = next_pairs_index.size() - 1; index_index > 0; --index_index)
	      {
		// compute index range in next index
		offset_min = search_index(next_pairs_index.at(index_index),
					  next_pair,
					  offset_min,
					  offset_max) * 512;

		offset_max = std::min(offset_min + 511,
				      next_pairs_index[index_index - 1].size() - 1);
	      }

	    offset_min = search_index(next_pairs_index.at(0),
				      next_pair,
				      offset_min,
				      offset_max) * 512;
	    offset_max = std::min(offset_min + 511,
				  next_layer_count - 1);

	    size_t next_rank_min = search_index(next_pairs,
						next_pair,
						offset_min,
						offset_max);
	    assert(next_pairs[next_rank_min] == next_pair);

	    return next_pair_rank_to_output[next_rank_min];
	  }
      };

      std::transform(
#ifdef __cpp_lib_parallel_algorithm
		     std::execution::par_unseq,
#endif
		     curr_transition_pairs.begin(),
		     curr_transition_pairs.end(),
		     curr_transitions.begin(),
		     calculate_backward_transition);

      profile.tic("backward transitions msync");

      sync_if_big<dfa_state_t>(curr_transitions);

      profile.tic("backward sort mmap");

      assert(curr_layer_count <= size_t(UINT32_MAX));
      MemoryMap<dfa_state_t> curr_pairs_permutation("scratch/binarydfa/pairs_permutation", curr_layer_count);
      for(dfa_state_t i = 0; i < curr_layer_count; ++i)
	{
	  curr_pairs_permutation[i] = i;
	}

      profile.tic("backward sort");

      auto compare_pair = [&](size_t curr_pair_index_a, size_t curr_pair_index_b)
      {
	return ::memcmp(&(curr_transitions[curr_pair_index_a * curr_layer_shape]),
			&(curr_transitions[curr_pair_index_b * curr_layer_shape]),
			sizeof(dfa_state_t) * curr_layer_shape) < 0;
      };

      // make permutation of pairs sorted by transitions

      sort<dfa_state_t>(curr_pairs_permutation.begin(),
			curr_pairs_permutation.end(),
			compare_pair);

      profile.tic("backward sort msync");

      sync_if_big<dfa_state_t>(curr_pairs_permutation);

      profile.tic("backward states");

      MemoryMap<dfa_state_t> curr_pair_rank_to_output = memory_map_helper<dfa_state_t>(layer % 2, "pair_rank_to_output", curr_layer_count);

      // figure out first two states used
      dfa_state_t curr_logical = ~dfa_state_t(0);
      dfa_state_t next_logical = 2; // next state to be set

      auto add_state_helper = [&](size_t curr_pairs_sorted_index)
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

      auto set_output_helper = [&](size_t curr_pairs_sorted_index)
      {
	dfa_state_t curr_rank = curr_pairs_permutation[curr_pairs_sorted_index];
	curr_pair_rank_to_output[curr_rank] = curr_logical;
      };

      add_state_helper(0);
      set_output_helper(0);

      for(size_t k = 1; k < curr_layer_count; ++k)
	{
	  if(compare_pair(curr_pairs_permutation[k-1], curr_pairs_permutation[k]))
	    {
	      add_state_helper(k);
	    }
	  set_output_helper(k);
	}

      assert(curr_pair_rank_to_output.size() == curr_layer_count);

      std::swap(next_pair_rank_to_output, curr_pair_rank_to_output);
      assert(next_pair_rank_to_output.size() > 0);

      // shrink state

      profile.tic("backward munmap");
      pairs_by_layer.pop_back();

      // cleanup in destructors
      profile.tic("backward cleanup");
    }
  profile.set_prefix("");

  assert(pairs_by_layer.size() == 1);

  assert(next_pair_rank_to_output.size() == 1);
  this->set_initial_state(next_pair_rank_to_output[0]);

  assert(this->ready());
  profile.tic("final cleanup");
}
