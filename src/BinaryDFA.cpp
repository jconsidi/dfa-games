// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <execution>
#include <iostream>
#include <memory>
#include <numeric>
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

static const size_t SYNC_THRESHOLD_BYTES = 1ULL << 25;

template<class T>
static void sync_if_big(MemoryMap<T>& memory_map)
{
  if(memory_map.length() >= SYNC_THRESHOLD_BYTES)
    {
      memory_map.msync();
    }
}

BinaryDFA::BinaryDFA(const dfa_shape_t& shape_in, const BinaryFunction& leaf_func_in)
  : DFA(shape_in),
    leaf_func(leaf_func_in)
{
}

BinaryDFA::BinaryDFA(const DFA& left_in,
                     const DFA& right_in,
                     const BinaryFunction& leaf_func_in)
  : BinaryDFA(left_in.get_shape(), leaf_func_in)
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
      build_linear(left_in, right_in);
      return;
    }
  else if(right_in.is_linear() &&
          leaf_func.is_commutative() &&
          leaf_func.has_right_sink(0))
    {
      // right side is linear DFA and confirmed commutative property
      build_linear(right_in, left_in);
      return;
    }

  // quadratic default
  build_quadratic(left_in, right_in);
}

static std::string binary_build_file_prefix(int layer)
{
  std::ostringstream filename_builder;
  filename_builder << "scratch/binarydfa/layer=" << (layer < 10 ? "0" : "") << layer;
  return filename_builder.str();
}

void BinaryDFA::build_linear(const DFA& left_in,
                             const DFA& right_in)
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

      int layer_shape = this->get_layer_shape(layer);

      std::function<dfa_state_t(dfa_state_t)> get_next_changed = [&](dfa_state_t next_state_in)
      {
        assert(right_reachable[layer+1].check(next_state_in));
        return changed_states[layer+1][next_index.rank(next_state_in)];
      };

      // rewrite all transitions for each state
      build_layer(layer, 2 + right_reachable[layer].count(), [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
      {
        assert(new_state_id >= 2);
        dfa_state_t reachable_rank = new_state_id - 2;
        dfa_state_t state_in = curr_reachable[reachable_rank];

        dfa_state_t transitions_max = 0;

        DFATransitionsReference transitions_in = right_in.get_transitions(layer, state_in);
        for(int i = 0; i < layer_shape; ++i)
          {
            if(left_filters[layer][i])
              {
                transitions_out[i] = get_next_changed(transitions_in[i]);
                transitions_max = std::max(transitions_max, transitions_out[i]);
              }
            else
              {
                transitions_out[i] = 0;
              }
          }

        if(transitions_max == 0)
          {
            // reject state
            changed_states[layer][reachable_rank] = 0;
            return;
          }
        if((transitions_max == 1) && (*std::min_element(transitions_out, transitions_out + layer_shape) == 1))
          {
            // accept state
            changed_states[layer][reachable_rank] = 1;
            return;
          }

        changed_states[layer][reachable_rank] = 2 + reachable_rank;
      });

      changed_states.pop_back();
      assert(changed_states.size() == layer + 1);
    }
  assert(changed_states.size() == 1);

  // done

  this->set_initial_state(changed_states[0][0]);
}

static std::string memory_map_name(int layer, std::string suffix)
{
  return binary_build_file_prefix(layer) + "-" + suffix;
}

template<class T>
static MemoryMap<T> memory_map_helper(int layer, std::string suffix, size_t size_in)
{
  return MemoryMap<T>(memory_map_name(layer, suffix), size_in);
}

void BinaryDFA::build_quadratic(const DFA& left_in,
                                const DFA& right_in)
{
  Profile profile("build_quadratic");

  // identify cases where leaf_func allows full evaluation without
  // going to leaves...

  auto shortcircuit_func = get_shortcircuit_func();
  auto filter_func = get_filter_func();

  dfa_state_t initial_left = left_in.get_initial_state();
  dfa_state_t initial_right = right_in.get_initial_state();
  if(filter_func(initial_left, initial_right))
    {
      this->set_initial_state(shortcircuit_func(initial_left, initial_right));
      return;
    }

  // forward pass

  profile.tic("forward");

  int num_layers_used = build_quadratic_forward(left_in, right_in);

  // backward pass

  profile.tic("backward");

  build_quadratic_backward(left_in, right_in, num_layers_used);

  assert(this->ready());
  profile.tic("final cleanup");
}

void BinaryDFA::build_quadratic_backward(const DFA& left_in,
                                         const DFA& right_in,
                                         int backward_layers)
{
  // initially empty since shortcircuiting will happen by last layer.

  MemoryMap<dfa_state_t> next_pair_rank_to_output(1); // dummy initialization

  // backward pass

  for(int layer = backward_layers - 1; layer >= 0; --layer)
    {
      MemoryMap<dfa_state_t> curr_pair_rank_to_output =
        build_quadratic_backward_layer(left_in,
                                       right_in,
                                       layer,
                                       next_pair_rank_to_output);

      next_pair_rank_to_output.unlink();
      std::swap(curr_pair_rank_to_output, next_pair_rank_to_output);
    }

  assert(next_pair_rank_to_output.size() == 1);
  this->set_initial_state(next_pair_rank_to_output[0]);
  next_pair_rank_to_output.unlink();
}

MemoryMap<dfa_state_t> BinaryDFA::build_quadratic_backward_layer(const DFA& left_in,
                                                                 const DFA& right_in,
                                                                 int layer,
                                                                 const MemoryMap<dfa_state_t>& next_pair_rank_to_output)
{
  Profile profile("build_quadratic_backward_layer");

  profile.set_prefix("layer=" + std::to_string(layer));
  profile.tic("backward init");

  int curr_layer_shape = this->get_layer_shape(layer);
  const MemoryMap<dfa_state_pair_t> curr_pairs = build_quadratic_read_pairs(layer);
  size_t curr_layer_count = curr_pairs.size();
  // not long term necessary, but guarantees that final DFA will fit within current limit.
  assert(curr_layer_count <= DFA_STATE_MAX);

  size_t next_left_size = left_in.get_layer_size(layer + 1);
  size_t next_right_size = right_in.get_layer_size(layer + 1);

  profile.tic("backward next pair mmap");

  const MemoryMap<dfa_state_pair_t> next_pairs = build_quadratic_read_pairs(layer + 1);
  size_t next_layer_count = next_pairs.size();

  profile.tic("backward next pair index");

  // index entries have first pair of 4KB block (64 bit pair)
  std::vector<MemoryMap<dfa_state_pair_t>> next_pairs_index;
  next_pairs_index.reserve(3);
  auto add_next_pairs_index = [&](const MemoryMap<dfa_state_pair_t>& previous_pairs)
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

  auto search_index = [&](const MemoryMap<dfa_state_pair_t>& next_pairs_index, dfa_state_pair_t next_pair, size_t offset_min, size_t offset_max)
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

  MemoryMap<dfa_state_pair_t> curr_transition_pairs = build_quadratic_transition_pairs(left_in, right_in, layer);

  profile.tic("backward transitions populate");

  auto filter_func = get_filter_func();
  auto shortcircuit_func = get_shortcircuit_func();

  MemoryMap<dfa_state_t> curr_transitions("scratch/binarydfa/transitions", curr_layer_count * curr_layer_shape, [&](size_t next_pair_index)
  {
    dfa_state_pair_t next_pair = curr_transition_pairs[next_pair_index];

    dfa_state_t next_left_state = next_pair.get_left_state();
    assert(next_left_state < next_left_size);
    dfa_state_t next_right_state = next_pair.get_right_state();
    assert(next_right_state < next_right_size);

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

  profile.tic("unlink transition_pairs");

  curr_transition_pairs.unlink();

  profile.tic("unlink next_pairs_index");

  while(next_pairs_index.size())
    {
      next_pairs_index.back().unlink();
      next_pairs_index.pop_back();
    }

  profile.tic("backward states identification");

  auto check_constant = [&](dfa_state_t curr_pair_rank)
  {
    dfa_state_t possible_constant = curr_transitions[size_t(curr_pair_rank) * curr_layer_shape];
    if(possible_constant >= 2)
      {
        return false;
      }

    for(int j = 1; j < curr_layer_shape; ++j)
      {
        if(curr_transitions[size_t(curr_pair_rank) * curr_layer_shape + j] != possible_constant)
          {
            return false;
          }
      }

    return true;
  };

  size_t layer_size = 2 + curr_layer_count;

  profile.tic("backward states write");

  auto curr_transitions_begin = curr_transitions.begin();

  auto populate_transitions = [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
  {
    assert(2 <= new_state_id);
    assert(new_state_id < layer_size);

    dfa_state_t curr_pair_rank = new_state_id - 2;

    std::copy_n(curr_transitions_begin + size_t(curr_pair_rank) * curr_layer_shape, curr_layer_shape, transitions_out);
  };

  build_layer(layer, layer_size, populate_transitions);

  profile.tic("backward output");

  MemoryMap<dfa_state_t> curr_pair_rank_to_output(binary_build_file_prefix(layer) + "-pair_rank_to_output", curr_layer_count, [&](size_t curr_pair_rank)
  {
    if(check_constant(curr_pair_rank))
      {
        return curr_transitions[size_t(curr_pair_rank) * curr_layer_shape];
      }

    assert(size_t(curr_pair_rank) + 2 <= DFA_STATE_MAX);
    return dfa_state_t(curr_pair_rank + 2);
  });

  assert(curr_pair_rank_to_output.size() > 0);

  // shrink state

  profile.tic("unlink transitions");

  curr_transitions.unlink();

  // cleanup in destructors
  profile.tic("backward cleanup");

  // done

  return curr_pair_rank_to_output;
}

int BinaryDFA::build_quadratic_forward(const DFA& left_in, const DFA& right_in)
{
  // this is the normal entry point for the forward pass.
  // it handles the special initialization of layer zero.

  // manual setup of initial layer

  dfa_state_t initial_left = left_in.get_initial_state();
  dfa_state_t initial_right = right_in.get_initial_state();

  MemoryMap<dfa_state_pair_t> initial_pairs = memory_map_helper<dfa_state_pair_t>(0, "pairs", 1);
  initial_pairs[0] = dfa_state_pair_t(initial_left, initial_right);

  return build_quadratic_forward(left_in, right_in, 0);
}

int BinaryDFA::build_quadratic_forward(const DFA& left_in, const DFA& right_in, int layer_min)
{
  // this does the bulk of the work in the forward pass, and supports
  // restarts. it assumes that at least layer zero was previously
  // built.

  Profile profile("build_quadratic_forward");

  assert(layer_min >= 0);

  // forward pass

  for(int layer = layer_min; layer < get_shape_size(); ++layer)
    {
      profile.tic("forward layer=" + std::to_string(layer));

      MemoryMap<dfa_state_pair_t> next_pairs = build_quadratic_forward_layer(left_in, right_in, layer);
      if(next_pairs.size() == 0)
        {
          // current layer had pairs. next one did not.
          return layer + 1;
        }

#ifdef PARANOIA
      profile.tic("forward next pairs paranoia");

      assert(TRY_PARALLEL_2(std::is_sorted, next_pairs.begin(), next_pairs.end()));
#endif

      profile.tic("forward cleanup");
    }

  assert(layer_min == get_shape_size());
  return layer_min;
}

MemoryMap<dfa_state_pair_t> BinaryDFA::build_quadratic_forward_layer(const DFA& left_in,
                                                                     const DFA& right_in,
                                                                     int layer)
{
  Profile profile("build_quadratic_forward_layer");
  profile.set_prefix("layer=" + std::to_string(layer));

  profile.tic("init");

  // get size of current layer and next layer.
  //
  // using size_t for product calculations...

  size_t next_left_size = left_in.get_layer_size(layer + 1);
  size_t next_right_size = right_in.get_layer_size(layer + 1);

  profile.tic("transitions");

  MemoryMap<dfa_state_pair_t> curr_transition_pairs = build_quadratic_transition_pairs(left_in, right_in, layer);

  std::cout << "pair count = " << curr_transition_pairs.size() << " (original)" << std::endl;

  // helper functions

  profile.tic("filter");

  auto filter_func = get_filter_func();

  auto remove_func = [&](dfa_state_pair_t next_pair)
  {
    dfa_state_t next_left_state = next_pair.get_left_state();
    dfa_state_t next_right_state = next_pair.get_right_state();

    return filter_func(next_left_state, next_right_state);
  };

  auto working_begin = curr_transition_pairs.begin();
  auto working_end = curr_transition_pairs.end();

  working_end = TRY_PARALLEL_3(std::remove_if,
                               working_begin,
                               working_end,
                               remove_func);

  profile.tic("pre-unique");

  working_end = TRY_PARALLEL_2(std::unique,
                               working_begin,
                               working_end);

  profile.tic("sort");

  TRY_PARALLEL_2(std::sort,
                 working_begin,
                 working_end);

  profile.tic("post-unique");

  working_end = TRY_PARALLEL_2(std::unique,
                               working_begin,
                               working_end);

  auto unique_end = working_end;

  std::cout << "pair count = " << (unique_end - curr_transition_pairs.begin()) << " (post sort unique)" << std::endl;

  // the following truncate and rename to the next pairs file are to
  // make the next pairs updates atomic and make restarts easier.

  profile.tic("munmap");

  // this munmap is implied by the truncate, but separating to track
  // the timing.
  curr_transition_pairs.munmap();

  profile.tic("truncate");

  size_t next_pairs_count = working_end - working_begin;
  curr_transition_pairs.truncate(next_pairs_count);

  profile.tic("forward stats");

  if(next_pairs_count >= 100000)
    {
      // stats compared to full quadratic blow up
      size_t bits_set = next_pairs_count;
      size_t bits_total = next_left_size * next_right_size;
      std::cout << "bits set = " << bits_set << " / " << bits_total << " ~ " << (double(bits_set) / double(bits_total)) << std::endl;
    }

  profile.tic("rename");

  std::string next_pairs_name = memory_map_name(layer + 1, "pairs");

  // atomic swap into place
  curr_transition_pairs.rename(next_pairs_name);

  profile.tic("done");

  return curr_transition_pairs;
}

MemoryMap<dfa_state_pair_t> BinaryDFA::build_quadratic_read_pairs(int layer)
{
  return MemoryMap<dfa_state_pair_t>(memory_map_name(layer, "pairs"));
}

MemoryMap<dfa_state_pair_t> BinaryDFA::build_quadratic_transition_pairs(const DFA& left_in,
                                                                        const DFA& right_in,
                                                                        int layer)
{
  Profile profile2("build_quadratic_transition_pairs");

  profile2.tic("init");

  int curr_layer_shape = this->get_layer_shape(layer);
  const MemoryMap<dfa_state_pair_t> curr_pairs = build_quadratic_read_pairs(layer);
  assert(curr_pairs.size() > 0);

  size_t curr_left_size = left_in.get_layer_size(layer);
  size_t curr_right_size = right_in.get_layer_size(layer);
  curr_pairs[curr_pairs.size() - 1].check(curr_left_size, curr_right_size);

  size_t transition_pairs_size = curr_pairs.size() * curr_layer_shape;

  // make sure inputs are memory mapped before going parallel
  curr_pairs.mmap();

  // read left transitions
  profile2.tic("left");

  left_in.get_transitions(layer, 0);
  MemoryMap<dfa_state_t> transition_pairs_left("scratch/binarydfa/transition_pairs_left", transition_pairs_size, [&](size_t transition_index)
  {
    size_t curr_i = transition_index / curr_layer_shape;
    size_t curr_j = transition_index % curr_layer_shape;

    dfa_state_pair_t curr_pair = curr_pairs[curr_i];
    dfa_state_t curr_left_state = curr_pair.get_left_state();
    assert(curr_left_state < curr_left_size);

    DFATransitionsReference left_transitions = left_in.get_transitions(layer, curr_left_state);
    return left_transitions.at(curr_j);
  });

  // read right transitions
  profile2.tic("right");

  right_in.get_transitions(layer, 0);

  MemoryMap<dfa_state_pair_t> curr_transition_pairs("scratch/binarydfa/transition_pairs", transition_pairs_size, [&](size_t transition_index)
  {
    size_t curr_i = transition_index / curr_layer_shape;
    size_t curr_j = transition_index % curr_layer_shape;

    dfa_state_pair_t curr_pair = curr_pairs[curr_i];
    dfa_state_t curr_right_state = curr_pair.get_right_state();
    assert(curr_right_state < curr_right_size);

    DFATransitionsReference right_transitions = right_in.get_transitions(layer, curr_right_state);
    return dfa_state_pair_t(transition_pairs_left[transition_index], right_transitions.at(curr_j));
  });

  // cleanup

  profile2.tic("left unlink");

  transition_pairs_left.unlink();

  profile2.tic("curr pairs munmap");

  curr_pairs.munmap();

  // done
  profile2.tic("done");
  return curr_transition_pairs;
};

std::function<bool(dfa_state_t, dfa_state_t)> BinaryDFA::get_filter_func() const
{
  // decide whether shortcircuit evaluation applies and we can filter
  // out these pairs from further evaluation.

  dfa_state_t left_sink = leaf_func.get_left_sink();
  dfa_state_t right_sink = leaf_func.get_right_sink();

  return [=](dfa_state_t left_in, dfa_state_t right_in)
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
}

std::function<dfa_state_t(dfa_state_t, dfa_state_t)> BinaryDFA::get_shortcircuit_func() const
{
  // apply shortcircuit logic to previously detected cases

  auto leaf_func_copy = leaf_func;

  dfa_state_t left_sink = leaf_func.get_left_sink();
  dfa_state_t right_sink = leaf_func.get_right_sink();

  return [=](dfa_state_t left_in, dfa_state_t right_in) -> dfa_state_t
  {
    if((left_in < 2) && (right_in < 2))
      {
        // constant inputs
        return leaf_func_copy(left_in, right_in);
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
}
