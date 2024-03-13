// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <execution>
#include <iostream>
#include <memory>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <queue>
#include <ranges>
#include <sstream>
#include <string>
#include <unistd.h>

#include "MemoryMap.h"
#include "Profile.h"
#include "VectorBitSet.h"
#include "parallel.h"

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

  left_in.mmap();
  right_in.mmap();

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

  profile.tic("linear states");

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

  profile.tic("reachable");

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

  profile.tic("states");

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

      // materialize because VectorBitSetIterator does not satisfy
      // std::random_access_iterator.
      MemoryMap<dfa_state_t> curr_reachable(right_reachable[layer].count());
      dfa_state_t curr_reachable_i = 0;
      for(auto iter = right_reachable[layer].begin();
	  iter < right_reachable[layer].end();
	  ++iter, ++curr_reachable_i)
	{
	  curr_reachable[curr_reachable_i] = dfa_state_t(*iter);
	}
      //VectorBitSetIndex curr_index(right_reachable[layer]);

      int layer_shape = this->get_layer_shape(layer);

      std::function<dfa_state_t(dfa_state_t)> get_next_changed = [&](dfa_state_t next_state_in)
      {
	assert(right_reachable[layer+1].check(next_state_in));
	return changed_states[layer+1][next_index.rank(next_state_in)];
      };

      const int transitions_temp_size = 100;
      assert(layer_shape <= transitions_temp_size);

      // loose allocation. may waste some states.
      set_layer_size(layer, 2 + right_reachable[layer].count());

      // rewrite all transitions for each state
      TRY_PARALLEL_3(std::for_each, curr_reachable.begin(), curr_reachable.end(), [&](const dfa_state_t& state_in)
	//TRY_PARALLEL_3(std::for_each, right_reachable[layer].begin(), right_reachable[layer].end(), [&](dfa_state_t state_in)
      {
	// creating fresh array to avoid sharing issues from parallelism.
	dfa_state_t transitions_temp[transitions_temp_size] = {0};
	dfa_state_t transitions_max = 0;

	dfa_state_t reachable_rank = &state_in - curr_reachable.begin();
	//dfa_state_t reachable_rank = curr_index.rank(state_in);

	DFATransitionsReference transitions_in = right_in.get_transitions(layer, state_in);
	for(int i = 0; i < layer_shape; ++i)
	  {
	    if(left_filters[layer][i])
	      {
		transitions_temp[i] = get_next_changed(transitions_in[i]);
		transitions_max = std::max(transitions_max, transitions_temp[i]);
	      }
	  }

	if(transitions_max == 0)
	  {
	    // reject state
	    changed_states[layer][reachable_rank] = 0;
	    return;
	  }
	if((transitions_max == 1) && (*std::min_element(transitions_temp, transitions_temp + layer_shape) == 1))
	  {
	    // accept state
	    changed_states[layer][reachable_rank] = 1;
	    return;
	  }

	set_state_transitions(layer, 2 + reachable_rank, transitions_temp);
	changed_states[layer][reachable_rank] = 2 + reachable_rank;
      });

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

    size_t transition_pairs_size = curr_pairs.size() * curr_layer_shape;

    // make sure inputs are memory mapped before going parallel
    curr_pairs.mmap();

    // read left transitions
    profile2.tic("left");

    size_t chunk_size = 1024;
    size_t chunks = (curr_pairs.size() + (chunk_size - 1)) / chunk_size;
    std::ranges::iota_view chunk_indexes(size_t(0), chunks);

    left_in.get_transitions(layer, 0);
    MemoryMap<dfa_state_t> transition_pairs_left("scratch/binarydfa/transition_pairs_left", transition_pairs_size);
    TRY_PARALLEL_3(std::for_each, chunk_indexes.begin(), chunk_indexes.end(), [&](size_t chunk_index)
    {
      size_t curr_i_min = chunk_index * chunk_size;
      size_t curr_i_max = std::min(curr_i_min + chunk_size,
                                   curr_pairs.size());

      for(size_t curr_i = curr_i_min;
          curr_i < curr_i_max;
          ++curr_i)
        {
          size_t curr_pair = curr_pairs[curr_i];
          size_t curr_left_state = curr_pair / curr_right_size;
          DFATransitionsReference left_transitions = left_in.get_transitions(layer, curr_left_state);

          for(int curr_j = 0; curr_j < curr_layer_shape; ++curr_j)
            {
              transition_pairs_left[curr_i * curr_layer_shape + curr_j] = left_transitions.at(curr_j);
            }
        }
    });

    // read right transitions
    profile2.tic("right");

    right_in.get_transitions(layer, 0);

    MemoryMap<size_t> curr_transition_pairs("scratch/binarydfa/transition_pairs", transition_pairs_size);
    TRY_PARALLEL_3(std::for_each, chunk_indexes.begin(), chunk_indexes.end(), [&](size_t chunk_index)
    {
      size_t curr_i_min = chunk_index * chunk_size;
      size_t curr_i_max = std::min(curr_i_min + chunk_size,
                                   curr_pairs.size());

      for(size_t curr_i = curr_i_min;
          curr_i < curr_i_max;
          ++curr_i)
        {
          size_t curr_pair = curr_pairs[curr_i];
          size_t curr_right_state = curr_pair % curr_right_size;
          DFATransitionsReference right_transitions = right_in.get_transitions(layer, curr_right_state);

          for(int curr_j = 0; curr_j < curr_layer_shape; ++curr_j)
            {
              size_t transition_index = curr_i * curr_layer_shape + curr_j;

              curr_transition_pairs[transition_index] = size_t(transition_pairs_left[transition_index]) * next_right_size + size_t(right_transitions.at(curr_j));
            }
        }
    });

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

      // helper functions

      profile.tic("forward transition pairs filter sort unique");

      auto remove_func = [&](size_t next_pair)
      {
        dfa_state_t next_left_state = next_pair / next_right_size;
        dfa_state_t next_right_state = next_pair % next_right_size;

        return filter_func(next_left_state, next_right_state);
      };

      const size_t working_block_max_elements = size_t(1) << 27;
      std::vector<size_t> working_block(std::min(working_block_max_elements, curr_transition_pairs.size()));

      size_t *unique_end = curr_transition_pairs.begin();

      auto check_next_pair = [&](size_t next_pair)
      {
	if(unique_end > curr_transition_pairs.begin())
	  {
	    assert(*(unique_end - 1) <= next_pair);
            if(*(unique_end - 1) == next_pair)
              {
                --unique_end;
              }
	  }
      };

      std::vector<std::tuple<size_t *, size_t *, size_t, size_t>> unique_queue;
      unique_queue.emplace_back(curr_transition_pairs.begin(),
				curr_transition_pairs.end(),
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
          size_t range_elements = (end - begin);
	  size_t range_bytes = range_elements * sizeof(size_t);
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
                  if(values_present.at(v - value_min) && !remove_func(v))
                    {
                      *unique_end++ = v;
                    }
                }

              continue;
            }

          if(range_elements <= working_block.size())
            {
              // "small" range, so just handle directly
              profile2.tic("base");

              // filtered copy from memory map
              auto working_begin = working_block.begin();
              auto working_end = TRY_PARALLEL_4(std::remove_copy_if, begin, end, working_begin, remove_func);
              assert(working_end <= working_block.end());
              if(working_end == working_begin)
                {
                  // everything was filtered
                  continue;
                }

	      TRY_PARALLEL_2(std::sort, working_begin, working_end);
              check_next_pair(*working_begin);

	      unique_end = TRY_PARALLEL_3(std::unique_copy, working_begin, working_end, unique_end);
              continue;
            }

	  // use flashsort permutation to partition by value

	  profile2.tic("partition");

          auto mid = begin + (end - begin + 1) / 2;
          assert(begin < mid);
          assert(mid < end);

          TRY_PARALLEL_3(std::nth_element,
                         begin,
                         mid,
                         end);
          auto value_mid = *mid;
          assert(value_min <= value_mid);
          assert(value_mid <= value_max);

          unique_queue.emplace_back(mid,
                                    end,
                                    value_mid,
                                    value_max);
          unique_queue.emplace_back(begin,
                                    mid,
                                    value_min,
                                    value_mid);
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

#ifdef PARANOIA
      profile.tic("forward next pairs paranoia");

      assert(TRY_PARALLEL_2(std::is_sorted, next_pairs.begin(), next_pairs.end()));
#endif

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
	next_pairs_index.emplace_back(index_name, index_length, [&](size_t i)
        {
          return previous_pairs[i * 512];
        });
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

      profile.tic("backward transitions populate");

      MemoryMap<dfa_state_t> curr_transitions("scratch/binarydfa/transitions", curr_layer_count * curr_layer_shape, [&](size_t next_pair_index)
      {
        size_t next_pair = curr_transition_pairs[next_pair_index];

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
      });

      profile.tic("backward transitions hash");

      MemoryMap<BinaryDFATransitionsHashPlusIndex> curr_transitions_hashed("scratch/binarydfa/transitions_hashed", curr_layer_count, [&](size_t i)
      {
        BinaryDFATransitionsHashPlusIndex output;
        if(curr_layer_shape + 1 <= binary_dfa_hash_width)
          {
            // copy transitions
            for(int j = 0; j < curr_layer_shape; ++j)
              {
                output.data[j] = curr_transitions[i * curr_layer_shape + j];
              }
            // and zero pad the rest of the hash space
            for(int j = curr_layer_shape; j < binary_dfa_hash_width - 1; ++j)
              {
                output.data[j] = 0;
              }
          }
        else
          {
            // transitions don't fit, so hash them

            unsigned char hash_output[SHA256_DIGEST_LENGTH];
            static const EVP_MD *hash_implementation = EVP_sha256();
            EVP_MD_CTX *hash_context = EVP_MD_CTX_create();

            EVP_DigestInit_ex(hash_context, hash_implementation, NULL);
            EVP_DigestUpdate(hash_context, &(curr_transitions[i * curr_layer_shape]), curr_layer_shape * sizeof(dfa_state_t));
            EVP_DigestFinal_ex(hash_context, hash_output, 0);

            for(int j = 0; j < binary_dfa_hash_width - 1; ++j)
              {
                output.data[j] = reinterpret_cast<dfa_state_t *>(hash_output)[j];
              }
          }

        output.data[binary_dfa_hash_width - 1] = i;

        return output;
      });

      profile.tic("backward sort hash sort");

      TRY_PARALLEL_2(std::sort, curr_transitions_hashed.begin(), curr_transitions_hashed.end());

      profile.tic("backward sort hash check");

      auto hash_collision = TRY_PARALLEL_3(std::adjacent_find, curr_transitions_hashed.begin(), curr_transitions_hashed.end(), [&](const BinaryDFATransitionsHashPlusIndex& a, const BinaryDFATransitionsHashPlusIndex& b)
      {
        // return true if hashes match but transitions do not

        if(a < b)
          {
            return false;
          }

        size_t curr_pair_index_a = a.get_pair_rank();
        size_t curr_pair_index_b = b.get_pair_rank();

        return ::memcmp(&(curr_transitions[curr_pair_index_a * curr_layer_shape]),
                        &(curr_transitions[curr_pair_index_b * curr_layer_shape]),
                        sizeof(dfa_state_t) * curr_layer_shape) != 0;
      });
      // confirm no mismatch found
      assert(hash_collision == curr_transitions_hashed.end());

      // make permutation of pairs sorted by transitions

      profile.tic("backward sort permutation");

      assert(curr_layer_count <= size_t(UINT32_MAX));
      MemoryMap<dfa_state_t> curr_pairs_permutation("scratch/binarydfa/pairs_permutation", curr_layer_count, [&](size_t i)
      {
        return curr_transitions_hashed[i].get_pair_rank();
      });

      profile.tic("backward states identification");

      MemoryMap<dfa_state_t> curr_pairs_permutation_to_output("scratch/binarydfa/pairs_permutation_to_output", curr_layer_count);

      auto check_constant = [&](dfa_state_t curr_pair_rank)
      {
	dfa_state_t possible_constant = curr_transitions[curr_pair_rank * curr_layer_shape];
	if(possible_constant >= 2)
	  {
	    return false;
	  }

	for(int j = 1; j < curr_layer_shape; ++j)
	  {
	    if(curr_transitions[curr_pair_rank * curr_layer_shape + j] != possible_constant)
	      {
		return false;
	      }
	  }

	return true;
      };

      auto curr_transitions_hashed_begin = curr_transitions_hashed.begin();
      auto check_new = [&](const BinaryDFATransitionsHashPlusIndex& curr_pair_hashed)
      {
        dfa_state_t curr_pair_rank = curr_pair_hashed.get_pair_rank();
	if(check_constant(curr_pair_rank))
	  {
	    return dfa_state_t(0);
	  }

	if(&curr_pair_hashed <= curr_transitions_hashed_begin)
	  {
	    // first and non-constant is always a new state
	    // TODO : change range to not require this
	    return dfa_state_t(1);
	  }

	if((&curr_pair_hashed)[-1] < curr_pair_hashed)
	  {
            // different transitions from predecessor
	    return dfa_state_t(1);
	  }

	return dfa_state_t(0);
      };

      TRY_PARALLEL_6(std::transform_inclusive_scan,
                     curr_transitions_hashed.begin(),
                     curr_transitions_hashed.end(),
		     curr_pairs_permutation_to_output.begin(),
		     [](dfa_state_t previous, dfa_state_t delta) {return previous + delta;},
		     check_new,
		     1); // first new state will be 2

      dfa_state_t layer_size = curr_pairs_permutation_to_output[curr_layer_count - 1] + 1;

      profile.tic("backward states resize");

      set_layer_size(layer, layer_size);

      profile.tic("backward states write");

      auto curr_pairs_permutation_to_output_begin = curr_pairs_permutation_to_output.begin();

      auto write_state = [&](const dfa_state_t& output_state)
      {
	if(output_state < 2)
	  {
	    return;
	  }

	dfa_state_t curr_pairs_permutation_index = &output_state - curr_pairs_permutation_to_output_begin;
	if(curr_pairs_permutation_index > 0)
	  {
	    if(*(&output_state - 1) == output_state)
	      {
		// not the first with this output state
		return;
	      }
	  }

	dfa_state_t curr_pair_rank = curr_pairs_permutation[curr_pairs_permutation_index];
	set_state_transitions(layer, output_state, &(curr_transitions[curr_pair_rank * curr_layer_shape]));
      };

      TRY_PARALLEL_3(std::for_each,
		     curr_pairs_permutation_to_output.begin(),
		     curr_pairs_permutation_to_output.end(),
		     write_state);

      profile.tic("backward invert");

      // invert permutation so we can write pair_rank_to_output in order

      MemoryMap<dfa_state_t> curr_pairs_permutation_inverse("scratch/binarydfa/pairs_permutation_inverse", curr_layer_count);
      // contents are indexes into curr_pairs_permutation
      std::iota(curr_pairs_permutation_inverse.begin(),
		curr_pairs_permutation_inverse.end(),
		0);
      // sort so curr_pairs_permutation[curr_pairs_permutation_inverse[i]] = i

      TRY_PARALLEL_3(std::sort,
		     curr_pairs_permutation_inverse.begin(),
		     curr_pairs_permutation_inverse.end(),
		     [&](int a, int b) {
		       return curr_pairs_permutation[a] < curr_pairs_permutation[b];
		     });

      for(dfa_state_t i : {size_t(0), curr_layer_count - 1})
	{
	  assert(curr_pairs_permutation[curr_pairs_permutation_inverse[i]] == i);
	}

      profile.tic("backward output");

      MemoryMap<dfa_state_t> curr_pair_rank_to_output(binary_build_file_prefix(layer % 2) + "-pair_rank_to_output", curr_layer_count, [&](size_t curr_pair_rank)
      {
        dfa_state_t curr_pairs_permutation_index = curr_pairs_permutation_inverse[curr_pair_rank];
	if(check_constant(curr_pair_rank))
	  {
	    return curr_transitions[curr_pair_rank * curr_layer_shape];
	  }

	return curr_pairs_permutation_to_output[curr_pairs_permutation_index];
      });

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
