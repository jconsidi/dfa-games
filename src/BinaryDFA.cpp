// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstring>
#include <execution>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <queue>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <vector>

#include "Flashsort.h"
#include "flash_sort.h"
#include "MemoryMap.h"
#include "Profile.h"
#include "ScratchConfig.h"
#include "VectorBitSet.h"
#include "parallel.h"
#include "sort_utils.h"

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
          this->set_canonical(true);
          this->set_initial_state(i);
          return;
        }

      if(right_in.is_constant(i) && leaf_func.has_right_sink(i))
        {
          this->set_canonical(true);
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

// Mirrors DFA.cpp's build_in_progress, for the same reason: proves "at most
// one binarydfa/ staging directory outstanding per process" instead of just
// observing it. Set when create_binary_directory creates one, cleared when
// DirectoryGuard's destructor removes it -- the guard's own scope already
// is this resource's entire lifetime, covering both a normal return and an
// exception unwinding through it, so one clear in the destructor is enough
// (unlike DFA's build_in_progress, which has two distinct fates -- saved vs
// abandoned -- this doesn't: every DirectoryGuard destruction means the
// directory is no longer needed, full stop).
static bool binary_build_in_progress = false;

// Working files for the BFS/quadratic construction below: pure scratch,
// unlinked within the same construction pass that writes them (or by
// DirectoryGuard if that pass throws instead), so they always belong on
// the fast local tier, never the archive. Bare pid, no counter -- same
// reasoning as DFA.cpp's get_temp_directory: binary_build_in_progress
// above makes it an asserted invariant that this process never has two of
// these outstanding at once, and a pid cannot be reused while this process
// holds it, so the pid alone already names a directory nothing else on the
// machine can collide with.
static std::string get_binary_directory()
{
  return (ScratchConfig::get_local_dir() + "/binarydfa/" +
	  std::to_string(getpid()));
}

BinaryDFA::DirectoryGuard::DirectoryGuard(std::string directory_in)
  : directory(std::move(directory_in))
{
}

BinaryDFA::DirectoryGuard::~DirectoryGuard() noexcept(false)
{
  binary_build_in_progress = false;
  remove_directory(directory);
}

BinaryDFA::DirectoryGuard BinaryDFA::create_binary_directory()
{
  assert(binary_directory.empty());

  assert(!binary_build_in_progress);
  binary_build_in_progress = true;

  binary_directory = create_directory(get_binary_directory());
  return DirectoryGuard(binary_directory);
}

std::string BinaryDFA::binary_dir() const
{
  return binary_directory;
}

std::string BinaryDFA::binary_build_file_prefix(int layer) const
{
  std::ostringstream filename_builder;
  filename_builder << binary_dir() << "/layer=" << (layer < 10 ? "0" : "") << layer;
  return filename_builder.str();
}

void BinaryDFA::build_linear(const DFA& left_in,
                             const DFA& right_in)
{
  Profile profile("build_linear");
  DirectoryGuard binary_directory_guard = create_binary_directory();

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

      for(auto curr_state: std::as_const(right_reachable[layer]))
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

  // Each reachable right state becomes output state 2 + its rank, or a
  // constant, so a layer's ids are below its reachable count + 2.
  std::vector<DFAStateIdMap> changed_states;
  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      size_t reachable_count = right_reachable.at(layer).count();
      changed_states.emplace_back(reachable_count, reachable_count + 2);
    }

  // terminal pseudo-layer
  changed_states.emplace_back(2, 2);
  changed_states[get_shape_size()].set(0, leaf_func(1, 0));
  changed_states[get_shape_size()].set(1, leaf_func(1, 1));

  // backward pass
  for(int layer = get_shape_size() - 1; layer >= 0; --layer)
    {
      VectorBitSetIndex next_index(right_reachable[layer+1]);

      // materialize because VectorBitSetIterator does not satisfy
      // std::random_access_iterator.
      DFAStateIdMap curr_reachable(right_reachable[layer].count(), right_in.get_layer_size(layer));
      dfa_state_t curr_reachable_i = 0;
      for(auto iter = right_reachable[layer].begin();
          iter < right_reachable[layer].end();
          ++iter, ++curr_reachable_i)
        {
          curr_reachable.set(curr_reachable_i, dfa_state_t(*iter));
        }

      int layer_shape = this->get_layer_shape(layer);

      std::function<dfa_state_t(dfa_state_t)> get_next_changed = [&](dfa_state_t next_state_in)
      {
        assert(right_reachable[layer+1].check(next_state_in));
        return changed_states[layer+1].get(next_index.rank(next_state_in));
      };

      // rewrite all transitions for each state
      build_layer(layer, 2 + right_reachable[layer].count(), [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
      {
        assert(new_state_id >= 2);
        dfa_state_t reachable_rank = new_state_id - 2;
        dfa_state_t state_in = curr_reachable.get(reachable_rank);

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
            changed_states[layer].set(reachable_rank, 0);
            return;
          }
        if((transitions_max == 1) && (*std::min_element(transitions_out, transitions_out + layer_shape) == 1))
          {
            // accept state
            changed_states[layer].set(reachable_rank, 1);
            return;
          }

        changed_states[layer].set(reachable_rank, 2 + reachable_rank);
      });

      changed_states.pop_back();
      assert(changed_states.size() == layer + 1);
    }
  assert(changed_states.size() == 1);

  // done

  this->set_initial_state(changed_states[0].get(0));
}

std::string BinaryDFA::memory_map_name(int layer, std::string suffix) const
{
  return binary_build_file_prefix(layer) + "-" + suffix;
}

template<class T>
MemoryMap<T> BinaryDFA::memory_map_helper(int layer, std::string suffix, size_t size_in) const
{
  return MemoryMap<T>(memory_map_name(layer, suffix), size_in);
}

void BinaryDFA::build_quadratic(const DFA& left_in,
                                const DFA& right_in)
{
  Profile profile("build_quadratic");
  DirectoryGuard binary_directory_guard = create_binary_directory();

  // identify cases where leaf_func allows full evaluation without
  // going to leaves...

  auto shortcircuit_func = get_shortcircuit_func();
  auto filter_func = get_filter_func();

  dfa_state_t initial_left = left_in.get_initial_state();
  dfa_state_t initial_right = right_in.get_initial_state();
  if(filter_func(initial_left, initial_right))
    {
      // No ordinary states, so canonical numbering is vacuous.
      this->set_canonical(true);
      this->set_initial_state(shortcircuit_func(initial_left, initial_right));
      return;
    }

  // forward pass

  profile.tic("forward");

  int num_layers_used = build_quadratic_forward(left_in, right_in);

  // backward pass

  profile.tic("backward");

  dfa_state_t initial_state = build_quadratic_backward(left_in, right_in, num_layers_used);

  // Every layer sorted by its raw transitions, so states came out numbered in
  // ascending order of those transitions, with uniform rows folded into the
  // reserved states and duplicates merged. That is canonical and minimal.
  //
  // Must be decided before set_initial_state, not after: that call finalizes
  // (and may immediately serialize) this DFA, and canonical is part of what
  // gets written.
  this->set_canonical(!hashed_any_layer);
  this->set_initial_state(initial_state);

  assert(this->ready());
  profile.tic("final cleanup");
}

// Calls f with std::type_identity<uint64_t> when wide, else with
// std::type_identity<uint32_t>, so one generic lambda covers both widths.
template <class F>
static decltype(auto) with_state_type(bool wide, F&& f)
{
  if(wide)
    {
      return f(std::type_identity<uint64_t>());
    }
  return f(std::type_identity<uint32_t>());
}

dfa_state_t BinaryDFA::build_quadratic_backward(const DFA& left_in,
                                                const DFA& right_in,
                                                int backward_layers)
{
  Profile profile("build_quadratic_backward");

  // initially empty since shortcircuiting will happen by last layer.

  DFAStateIdMap next_pair_rank_to_output(MemoryMap<uint32_t>(1)); // dummy initialization

  // backward pass

  for(int layer = backward_layers - 1; layer >= 0; --layer)
    {
      profile.tic("layer=" + std::to_string(layer));

      DFAStateIdMap curr_pair_rank_to_output =
        build_quadratic_backward_layer(left_in,
                                       right_in,
                                       layer,
                                       next_pair_rank_to_output);

      next_pair_rank_to_output.unlink();
      std::swap(curr_pair_rank_to_output, next_pair_rank_to_output);
    }

  assert(next_pair_rank_to_output.size() == 1);
  dfa_state_t initial_state = next_pair_rank_to_output.get(0);
  next_pair_rank_to_output.unlink();
  return initial_state;
}

// Largest layer shape the row sort path handles. Each shape is its own
// record type, so this bounds the instantiations.
static const int binary_dfa_row_shape_max = 4;

// Calls f with std::integral_constant<int, shape> for shapes 1 to
// binary_dfa_row_shape_max.
template <class F>
static decltype(auto) with_row_shape(int shape, F&& f)
{
  switch(shape)
    {
    case 1:
      return f(std::integral_constant<int, 1>());
    case 2:
      return f(std::integral_constant<int, 2>());
    case 3:
      return f(std::integral_constant<int, 3>());
    case 4:
      return f(std::integral_constant<int, 4>());
    }
  static_assert(binary_dfa_row_shape_max == 4);
  throw std::logic_error("with_row_shape: shape " + std::to_string(shape) + " outside 1 to " + std::to_string(binary_dfa_row_shape_max));
}

size_t binary_dfa_narrow_pair_count_max = UINT32_MAX;
size_t binary_dfa_output_slices_min = 1;
uint64_t binary_dfa_key_narrow_max = UINT32_MAX;
bool binary_dfa_rows_enabled = true;

DFAStateIdMap BinaryDFA::build_quadratic_backward_layer(const DFA& left_in,
                                                              const DFA& right_in,
                                                              int layer,
                                                              const DFAStateIdMap& next_pair_rank_to_output)
{
  size_t curr_layer_count = build_quadratic_pair_count(left_in, right_in, layer);
  assert(curr_layer_count > 0);

  if(binary_dfa_narrow_pair_count_max > BinaryDFATransitionsHashPlusIndex<false>::pair_rank_max)
    {
      throw std::logic_error("BinaryDFA: binary_dfa_narrow_pair_count_max " + std::to_string(binary_dfa_narrow_pair_count_max) + " exceeds what narrow pair ranks can hold");
    }

  bool wide_ranks = curr_layer_count > binary_dfa_narrow_pair_count_max;
  if(wide_ranks && (curr_layer_count - 1 > BinaryDFATransitionsHashPlusIndex<true>::pair_rank_max))
    {
      throw std::overflow_error("BinaryDFA: layer " + std::to_string(layer) + " has " + std::to_string(curr_layer_count) + " pairs, more than wide pair ranks can number");
    }

  bool curr_pairs_wide = build_quadratic_pairs_wide(left_in, right_in, layer);
  bool next_pairs_wide = build_quadratic_pairs_wide(left_in, right_in, layer + 1);

  return next_pair_rank_to_output.visit([&](const auto& next_ids) -> DFAStateIdMap
  {
    typedef typename std::remove_cvref_t<decltype(next_ids)> next_ids_t;
    typedef std::remove_cvref_t<decltype(next_ids[0])> next_id_t;
    static_assert(std::is_same_v<next_ids_t, MemoryMap<next_id_t>>);

    return with_state_type(next_pairs_wide, [&](auto next_tag) -> DFAStateIdMap
    {
      typedef dfa_state_pair<typename decltype(next_tag)::type> next_pair_t;

      if(wide_ranks)
        {
          return build_quadratic_backward_layer<true, next_pair_t, next_id_t>(left_in, right_in, layer, curr_layer_count, next_ids, curr_pairs_wide);
        }
      return build_quadratic_backward_layer<false, next_pair_t, next_id_t>(left_in, right_in, layer, curr_layer_count, next_ids, curr_pairs_wide);
    });
  });
}

template <bool wide, class next_pair_t, class next_id_t>
DFAStateIdMap BinaryDFA::build_quadratic_backward_layer(const DFA& left_in,
                                                              const DFA& right_in,
                                                              int layer,
                                                              size_t curr_layer_count,
                                                              const MemoryMap<next_id_t>& next_pair_rank_to_output,
                                                              bool curr_pairs_wide)
{
  typedef BinaryDFATransitionsHashPlusIndex<wide> hash_plus_index_t;
  // Ranks of pairs in this layer, which may exceed 32 bits in wide mode.
  typedef std::conditional_t<wide, size_t, uint32_t> pair_rank_t;

  Profile profile("build_quadratic_backward_layer");

  profile.set_prefix("layer=" + std::to_string(layer));
  profile.tic(wide ? "init wide" : "init");

  int curr_layer_shape = this->get_layer_shape(layer);
  assert(curr_layer_count - 1 <= hash_plus_index_t::pair_rank_max);

  size_t next_left_size = left_in.get_layer_size(layer + 1);
  size_t next_right_size = right_in.get_layer_size(layer + 1);

  // Every transition below is an output state of layer + 1, already built.
  // Keys depend on that layer's size alone, never on next_id_t, so a layer
  // numbers its states the same whatever width its arrays are stored at.
  size_t next_layer_size = this->get_layer_size(layer + 1);
  bool key_narrow = uint64_t(next_layer_size - 1) <= binary_dfa_key_narrow_max;

  profile.tic("next pair mmap");

  // The next layer's pairs and their outputs, merged in rank order so each
  // lookup below finds its pair and reads its output from one block. Both
  // inputs are read sequentially.
  typedef dfa_pair_output<next_pair_t, next_id_t> next_pair_output_t;
  size_t next_layer_count = 0;
  MemoryMap<next_pair_output_t> next_pair_outputs = [&]()
  {
    MemoryMap<next_pair_t> next_pairs = build_quadratic_read_pairs<next_pair_t>(layer + 1);
    next_layer_count = next_pairs.size();
    // A next layer without pairs is past the top of the backward pass, every
    // transition into it short circuits, and its outputs are the one entry
    // placeholder build_quadratic_backward starts from.
    size_t outputs_expected = (next_layer_count > 0) ? next_layer_count : 1;
    if(next_pair_rank_to_output.size() != outputs_expected)
      {
        throw std::logic_error("BinaryDFA: layer " + std::to_string(layer + 1) + " has " + std::to_string(next_layer_count) + " pairs but " + std::to_string(next_pair_rank_to_output.size()) + " pair outputs");
      }

    profile.tic("next pair outputs");
    MemoryMap<next_pair_output_t> merged(binary_dir() + "/next_pair_outputs", next_layer_count, [&](size_t next_pair_rank)
    {
      return next_pair_output_t{next_pairs[next_pair_rank], next_pair_rank_to_output[next_pair_rank]};
    });

    // The merged file holds every next pair, and this is the last layer that
    // reads them, so the next layer's pairs file is unlinked here.
    profile.tic("unlink next pairs");
    next_pairs.unlink();

    return merged;
  }();

  profile.tic("next pair index");

  // index entries have the first pair of each 512 entry block
  std::vector<MemoryMap<next_pair_t>> next_pairs_index;
  next_pairs_index.reserve(3);
  auto add_next_pairs_index = [&](size_t previous_size, auto previous_pair)
  {
    std::string index_name = binary_dir() + "/next_pairs_index-" + std::to_string(next_pairs_index.size());
    size_t index_length = (previous_size + 511) / 512;
    next_pairs_index.emplace_back(index_name, index_length, [&](size_t i)
    {
      return previous_pair(i * 512);
    });
  };
  add_next_pairs_index(next_layer_count, [&](size_t i) {return next_pair_outputs[i].pair;});
  assert(next_pairs_index.size() == 1);

  // add more indexes until under 1MB
  while(next_pairs_index.back().length() > 1ULL << 20)
    {
      assert(next_pairs_index.size() < next_pairs_index.capacity());
      const MemoryMap<next_pair_t>& previous_index = next_pairs_index.back();
      add_next_pairs_index(previous_index.size(), [&](size_t i) {return previous_index[i];});
    }

  // Last offset in [offset_min, offset_max] whose pair is at most
  // next_pair, reading pairs through pair_at.
  auto search = [&](auto pair_at, next_pair_t next_pair, size_t offset_min, size_t offset_max)
  {
    assert(offset_min <= offset_max);

    while(offset_min < offset_max)
      {
        size_t offset_mid = offset_min + (offset_max - offset_min + 1) / 2;
        if(pair_at(offset_mid) <= next_pair)
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

  profile.tic("transitions input");

  MemoryMap<next_pair_t> curr_transition_pairs = with_state_type(curr_pairs_wide, [&](auto curr_tag)
  {
    typedef dfa_state_pair<typename decltype(curr_tag)::type> curr_pair_t;
    return build_quadratic_transition_pairs<curr_pair_t, next_pair_t>(left_in, right_in, layer);
  });

  profile.tic("transitions populate");

  auto filter_func = get_filter_func();
  auto shortcircuit_func = get_shortcircuit_func();

  auto transition_for = [&](size_t next_pair_index)
  {
    next_pair_t next_pair = curr_transition_pairs[next_pair_index];

    dfa_state_t next_left_state = next_pair.get_left_state();
    assert(next_left_state < next_left_size);
    dfa_state_t next_right_state = next_pair.get_right_state();
    assert(next_right_state < next_right_size);

    if(filter_func(next_left_state, next_right_state))
      {
        dfa_state_t shortcircuit_state = shortcircuit_func(next_left_state, next_right_state);
        assert(shortcircuit_state < 2);
        return next_id_t(shortcircuit_state);
      }
    else
      {
        auto search_index = [&](const MemoryMap<next_pair_t>& index, size_t offset_min, size_t offset_max)
        {
          assert(offset_max < index.size());
          return search([&](size_t i) {return index[i];}, next_pair, offset_min, offset_max);
        };

        size_t offset_min = 0;
        size_t offset_max = next_pairs_index.back().size() - 1;

        for(size_t index_index = next_pairs_index.size() - 1; index_index > 0; --index_index)
          {
            // compute index range in next index
            offset_min = search_index(next_pairs_index.at(index_index),
                                      offset_min,
                                      offset_max) * 512;

            offset_max = std::min(offset_min + 511,
                                  next_pairs_index[index_index - 1].size() - 1);
          }

        offset_min = search_index(next_pairs_index.at(0),
                                  offset_min,
                                  offset_max) * 512;
        offset_max = std::min(offset_min + 511,
                              next_layer_count - 1);

        size_t next_pair_rank = search([&](size_t i) {return next_pair_outputs[i].pair;},
                                       next_pair,
                                       offset_min,
                                       offset_max);
        const next_pair_output_t& found = next_pair_outputs[next_pair_rank];
        assert(found.pair == next_pair);

        return found.output;
      }
  };

  // Up to direct_max transitions that each fit 32 bits, the sort key is the
  // transitions themselves, so the sort leaves states in canonical order
  // (FORMAT-DFA.md section 8) and the scan below numbers them 2, 3, ... in
  // that order. Otherwise the key is a hash and the order is arbitrary.
  bool direct_key = key_narrow && (curr_layer_shape <= hash_plus_index_t::direct_max);

  std::string state_bytes = std::to_string(sizeof(typename next_pair_t::state_type)) + " next pairs, " + std::to_string(sizeof(next_id_t)) + " next ids";

  // Every transition has been read once the chosen path has its rows, so
  // it calls this to drop the inputs they were read from.
  auto release_inputs = [&]()
  {
    profile.tic("unlink transition_pairs");

    curr_transition_pairs.unlink();

    profile.tic("unlink next_pairs_index");

    while(next_pairs_index.size())
      {
        next_pairs_index.back().unlink();
        next_pairs_index.pop_back();
      }

    profile.tic("unlink next_pair_outputs");

    next_pair_outputs.unlink();
  };

  // Rows sorted as themselves number states the same way direct keys do, so
  // they replace the hashed path wherever its keys are direct, keeping those
  // layers byte identical, and wherever the next layer is too big for 4 byte
  // keys, which only wide layers reach. Each record carries its row, so
  // every step after the sort reads the rows in order.
  if(binary_dfa_rows_enabled && (curr_layer_shape <= binary_dfa_row_shape_max) && (direct_key || !key_narrow))
    {
      return with_row_shape(curr_layer_shape, [&](auto shape_tag) -> DFAStateIdMap
      {
        // Fixed width, so a record type is the same on every platform.
        typedef std::conditional_t<wide, uint64_t, uint32_t> record_rank_t;
        static_assert(sizeof(record_rank_t) >= sizeof(pair_rank_t));
        typedef RowRankRecord<next_id_t, decltype(shape_tag)::value, record_rank_t> record_t;
        return build_quadratic_backward_layer_rows<record_t>(layer, curr_layer_count, transition_for, release_inputs, state_bytes, profile);
      });
    }

  return build_quadratic_backward_layer_hashed<wide, next_id_t>(layer, curr_layer_count, key_narrow, transition_for, release_inputs, state_bytes, profile);
}

// The row sort path of build_quadratic_backward_layer: each pair's row and
// rank as one record, sorted by the row, so equal rows are found exactly
// and every step after the sort reads the rows in order. States are
// numbered in sorted row order, as direct keys number them.
template <class record_t, class F>
DFAStateIdMap BinaryDFA::build_quadratic_backward_layer_rows(int layer,
                                                             size_t curr_layer_count,
                                                             const F& transition_for,
                                                             const std::function<void()>& release_inputs,
                                                             const std::string& state_bytes,
                                                             Profile& profile)
{
  constexpr int shape = record_t::row_shape;
  typedef typename record_t::rank_type pair_rank_t;
  assert(shape == this->get_layer_shape(layer));

  MemoryMap<record_t> rows(binary_dir() + "/rows", curr_layer_count, [&](size_t curr_pair_rank)
  {
    record_t record;
    for(int j = 0; j < shape; ++j)
      {
        record.row[j] = transition_for(curr_pair_rank * shape + size_t(j));
      }
    record.rank = pair_rank_t(curr_pair_rank);
    return record;
  });

  release_inputs();

  profile.tic("rows sort");

  FlashSort<record_t>().sort(rows.begin(), rows.end());

  profile.tic("states identification");

  const record_t *rows_begin = rows.begin();

  // 0 or 1 for a row of all reject or all accept transitions, else -1.
  auto constant_of = [](const record_t& record) -> int
  {
    if(record.row[0] >= 2)
      {
        return -1;
      }
    for(int j = 1; j < shape; ++j)
      {
        if(record.row[j] != record.row[0])
          {
            return -1;
          }
      }
    return int(record.row[0]);
  };

  auto check_new = [&](const record_t& record) -> int
  {
    if(constant_of(record) >= 0)
      {
        return 0;
      }
    if(&record == rows_begin)
      {
        return 1;
      }
    return (&record)[-1].row_equal(record) ? 0 : 1;
  };

  auto check_new_count = [&](const record_t& record)
  {
    return size_t(check_new(record));
  };
  size_t new_states = TRY_PARALLEL_5(std::transform_reduce,
                                     rows.begin(),
                                     rows.end(),
                                     size_t(0),
                                     std::plus<size_t>(),
                                     check_new_count);
  size_t layer_size_expected = new_states + 2;

  return with_state_type(dfa_state_count_is_wide(layer_size_expected), [&](auto curr_tag) -> DFAStateIdMap
  {
    typedef typename decltype(curr_tag)::type curr_id_t;

    MemoryMap<curr_id_t> curr_pairs_permutation_to_output(binary_dir() + "/pairs_permutation_to_output", curr_layer_count);

    auto check_new_id = [&](const record_t& record)
    {
      return curr_id_t(check_new(record));
    };

    TRY_PARALLEL_6(std::transform_inclusive_scan,
                   rows.begin(),
                   rows.end(),
                   curr_pairs_permutation_to_output.begin(),
                   [](curr_id_t previous, curr_id_t delta) {
                     curr_id_t output = previous + delta;
                     // check for overflow
                     assert(output >= previous);
                     return output;
                   },
                   check_new_id,
                   curr_id_t(1)); // first new state will be 2

    size_t layer_size = size_t(curr_pairs_permutation_to_output[curr_layer_count - 1]) + 1;
    assert(layer_size == layer_size_expected);

    profile.tic("states positions");

    // Sorted position of each new state's row, so states write reads
    // the rows in order. Constant rows get their constant here, so
    // nothing patches them later.
    MemoryMap<size_t> state_positions(binary_dir() + "/state_positions", std::max(new_states, size_t(1)));
    size_t *state_positions_data = state_positions.begin();
    curr_id_t *permutation_to_output_data = curr_pairs_permutation_to_output.begin();

    TRY_PARALLEL_3(std::for_each, rows.begin(), rows.end(), [&](const record_t& record)
    {
      size_t i = size_t(&record - rows_begin);
      int constant = constant_of(record);
      if(constant >= 0)
        {
          permutation_to_output_data[i] = curr_id_t(constant);
          return;
        }
      if(check_new(record))
        {
          size_t id = size_t(permutation_to_output_data[i]);
          assert((2 <= id) && (id < layer_size));
          state_positions_data[id - 2] = i;
        }
    });

    profile.tic("states write");

    auto populate_transitions = [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
    {
      assert((2 <= new_state_id) && (new_state_id < layer_size));
      size_t i = state_positions_data[new_state_id - 2];
      assert(i < curr_layer_count);
      assert(size_t(permutation_to_output_data[i]) == new_state_id);
      const record_t& record = rows_begin[i];
      for(int j = 0; j < shape; ++j)
        {
          transitions_out[j] = dfa_state_t(record.row[j]);
        }
    };

    build_layer(layer, layer_size, populate_transitions);

    profile.tic("unlink state_positions");

    state_positions.unlink();

    // pair_rank_to_output in rank order: each sorted row's rank and output
    // id as one record, sorted by the rank, so every pass over them reads
    // and writes in order.
    typedef RankOutputRecord<pair_rank_t, curr_id_t> rank_output_t;

    profile.tic("rank outputs");

    MemoryMap<rank_output_t> rank_outputs(binary_dir() + "/rank_outputs", curr_layer_count, [&](size_t i)
    {
      rank_output_t record;
      record.row[0] = rows_begin[i].rank;
      record.rank = permutation_to_output_data[i];
      return record;
    });

    profile.tic("unlink rows");

    rows.unlink();

    profile.tic("unlink pairs_permutation_to_output");

    curr_pairs_permutation_to_output.unlink();

    profile.tic("rank outputs sort");

    FlashSort<rank_output_t>().sort(rank_outputs.begin(), rank_outputs.end());

    profile.tic("rank outputs check");

    // The ranks are a permutation of 0 to curr_layer_count - 1 exactly
    // when sorted record k holds rank k.
    const rank_output_t *rank_outputs_begin = rank_outputs.begin();
    auto rank_misplaced = [&](const rank_output_t& record)
    {
      return size_t(record.row[0]) != size_t(&record - rank_outputs_begin);
    };
    const rank_output_t *misplaced = TRY_PARALLEL_3(std::find_if, rank_outputs.begin(), rank_outputs.end(), rank_misplaced);
    if(misplaced != rank_outputs.end())
      {
        size_t k = size_t(misplaced - rank_outputs_begin);
        throw std::logic_error("BinaryDFA: layer " + std::to_string(layer) + " sorted rank outputs hold rank " + std::to_string(misplaced->row[0]) + " at position " + std::to_string(k) + ", so pair rank " + std::to_string(k) + " is missing or repeated");
      }

    profile.tic("output write");

    std::cout << "layer=" << layer << " pair_rank_to_output: " << curr_layer_count << " ranks written in order after a rank sort; state bytes: " << state_bytes << ", " << sizeof(curr_id_t) << " ids, rows" << std::endl;

    MemoryMap<curr_id_t> curr_pair_rank_to_output(binary_build_file_prefix(layer) + "-pair_rank_to_output", curr_layer_count, [&](size_t curr_pair_rank)
    {
      return rank_outputs_begin[curr_pair_rank].rank;
    });

    profile.tic("unlink rank_outputs");

    rank_outputs.unlink();

    profile.tic("cleanup");

    return DFAStateIdMap(std::move(curr_pair_rank_to_output));
  });
}

// The hashed path of build_quadratic_backward_layer: transitions stored by
// rank, a 16 byte key per row (its transitions when they fit, else a
// SHA-256 of them), sorted, with equal keys checked against the rows.
template <bool wide, class next_id_t, class F>
DFAStateIdMap BinaryDFA::build_quadratic_backward_layer_hashed(int layer,
                                                               size_t curr_layer_count,
                                                               bool key_narrow,
                                                               const F& transition_for,
                                                               const std::function<void()>& release_inputs,
                                                               const std::string& state_bytes,
                                                               Profile& profile)
{
  typedef BinaryDFATransitionsHashPlusIndex<wide> hash_plus_index_t;
  // Ranks of pairs in this layer, which may exceed 32 bits in wide mode.
  typedef std::conditional_t<wide, size_t, uint32_t> pair_rank_t;

  int curr_layer_shape = this->get_layer_shape(layer);
  int key_bytes = key_narrow ? 4 : 8;

  // Up to direct_max transitions that each fit 32 bits, the sort key is the
  // transitions themselves, so the sort leaves states in canonical order
  // (FORMAT-DFA.md section 8) and the scan below numbers them 2, 3, ... in
  // that order. Otherwise the key is a hash and the order is arbitrary.
  bool direct_key = key_narrow && (curr_layer_shape <= hash_plus_index_t::direct_max);

  MemoryMap<next_id_t> curr_transitions(binary_dir() + "/transitions", curr_layer_count * curr_layer_shape, transition_for);

  release_inputs();

  profile.tic("transitions hash");

  if(!direct_key)
    {
      hashed_any_layer = true;
    }

  auto check_constant = [&](size_t curr_pair_rank)
  {
    next_id_t possible_constant = curr_transitions[curr_pair_rank * curr_layer_shape];
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

  // Constant rows (every transition 0, or every transition 1) get a fixed key
  // per constant, so the scan below recognizes them from the sorted record
  // alone instead of reading their rows at random ranks. With direct keys
  // that is just their transitions, which no non-constant row can share.
  // With hashed keys it is a reserved value {c, 0, 0}, and a non-constant
  // row whose hash lands on one is rejected below rather than taken for a
  // constant.
  hash_plus_index_t constant_records[2];
  uint32_t constant_keys[2][hash_plus_index_t::key_words];
  for(uint32_t c : {uint32_t(0), uint32_t(1)})
    {
      for(int j = 0; j < hash_plus_index_t::key_words; ++j)
        {
          if(direct_key)
            {
              constant_keys[c][j] = (j < curr_layer_shape) ? c : 0;
            }
          else
            {
              constant_keys[c][j] = (j == 0) ? c : 0;
            }
        }
      constant_records[c].set(constant_keys[c], 0);
    }
  assert(!(constant_records[0] == constant_records[1]));

  auto is_constant_record = [&](const hash_plus_index_t& record)
  {
    return (record == constant_records[0]) || (record == constant_records[1]);
  };

  // Hashed keys cover each transition at key_bytes. When that is how the
  // transitions are already stored, the row is hashed in place, which is
  // every narrow layer in production; otherwise each row is copied out at
  // key_bytes first.
  bool hash_in_place = (key_bytes == int(sizeof(next_id_t)));

  std::atomic<bool> reserved_key_collision(false);
  MemoryMap<hash_plus_index_t> curr_transitions_hashed(binary_dir() + "/transitions_hashed", curr_layer_count, [&](size_t i)
  {
    hash_plus_index_t output;
    if(check_constant(i))
      {
        output.set(constant_keys[curr_transitions[i * curr_layer_shape]], i);
        return output;
      }

    uint32_t key[hash_plus_index_t::key_words];
    if(direct_key)
      {
        // copy transitions
        for(int j = 0; j < curr_layer_shape; ++j)
          {
            next_id_t transition = curr_transitions[i * curr_layer_shape + j];
            assert(std::in_range<uint32_t>(transition));
            key[j] = uint32_t(transition);
          }
        // and zero pad the rest of the hash space
        for(int j = curr_layer_shape; j < hash_plus_index_t::key_words; ++j)
          {
            key[j] = 0;
          }
      }
    else
      {
        // transitions don't fit, so hash them

        const next_id_t *row = &(curr_transitions[i * curr_layer_shape]);
        const void *hash_input = row;
        size_t hash_input_bytes = size_t(curr_layer_shape) * sizeof(next_id_t);

        thread_local std::vector<uint8_t> row_bytes;
        if(!hash_in_place)
          {
            row_bytes.resize(size_t(curr_layer_shape) * size_t(key_bytes));
            for(int j = 0; j < curr_layer_shape; ++j)
              {
                if(key_narrow)
                  {
                    assert(std::in_range<uint32_t>(row[j]));
                    uint32_t transition = uint32_t(row[j]);
                    memcpy(row_bytes.data() + size_t(j) * 4, &transition, 4);
                  }
                else
                  {
                    uint64_t transition = uint64_t(row[j]);
                    memcpy(row_bytes.data() + size_t(j) * 8, &transition, 8);
                  }
              }
            hash_input = row_bytes.data();
            hash_input_bytes = row_bytes.size();
          }

        unsigned char hash_output[SHA256_DIGEST_LENGTH];
        static const EVP_MD *hash_implementation = EVP_sha256();
        EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
        bool hash_ok = ((hash_context != NULL) &&
                        (EVP_DigestInit_ex(hash_context, hash_implementation, NULL) == 1) &&
                        (EVP_DigestUpdate(hash_context, hash_input, hash_input_bytes) == 1) &&
                        (EVP_DigestFinal_ex(hash_context, hash_output, 0) == 1));
        EVP_MD_CTX_destroy(hash_context);
        if(!hash_ok)
          {
            throw std::runtime_error("BinaryDFA: SHA-256 of layer " + std::to_string(layer) + " transitions failed");
          }

        static_assert(hash_plus_index_t::key_words * sizeof(uint32_t) <= SHA256_DIGEST_LENGTH);
        for(int j = 0; j < hash_plus_index_t::key_words; ++j)
          {
            key[j] = reinterpret_cast<uint32_t *>(hash_output)[j];
          }
      }

    output.set(key, i);
    if(is_constant_record(output))
      {
        reserved_key_collision = true;
      }

    return output;
  });

  if(reserved_key_collision)
    {
      throw std::logic_error("BinaryDFA: layer " + std::to_string(layer) + " has a non-constant row whose sort key equals a constant row's");
    }

  profile.tic("sort hash");

  parallel_sort(curr_transitions_hashed.begin(), curr_transitions_hashed.end());

  profile.tic("sort hash check");

  auto hash_collision = TRY_PARALLEL_3(std::adjacent_find, curr_transitions_hashed.begin(), curr_transitions_hashed.end(), [&](const hash_plus_index_t& a, const hash_plus_index_t& b)
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
                    sizeof(next_id_t) * curr_layer_shape) != 0;
  });
  // confirm no mismatch found
  assert(hash_collision == curr_transitions_hashed.end());

  // make permutation of pairs sorted by transitions

  profile.tic("sort permutation");

  MemoryMap<pair_rank_t> curr_pairs_permutation(binary_dir() + "/pairs_permutation", curr_layer_count, [&](size_t i)
  {
    return pair_rank_t(curr_transitions_hashed[i].get_pair_rank());
  });

  profile.tic("states identification");

  auto curr_transitions_hashed_begin = curr_transitions_hashed.begin();
  auto check_new = [&](const hash_plus_index_t& curr_pair_hashed)
  {
    if(is_constant_record(curr_pair_hashed))
      {
        return 0;
      }

    if(&curr_pair_hashed <= curr_transitions_hashed_begin)
      {
        // first and non-constant is always a new state
        // TODO : change range to not require this
        return 1;
      }

    if((&curr_pair_hashed)[-1] < curr_pair_hashed)
      {
        // different transitions from predecessor
        return 1;
      }

    return 0;
  };

  // Count the new states first, in size_t, so the ids below can be numbered
  // at a width known to hold them. Ids run 2 .. new_states + 1.
  auto check_new_count = [&](const hash_plus_index_t& curr_pair_hashed)
  {
    return size_t(check_new(curr_pair_hashed));
  };
  size_t new_states = TRY_PARALLEL_5(std::transform_reduce,
                                     curr_transitions_hashed.begin(),
                                     curr_transitions_hashed.end(),
                                     size_t(0),
                                     std::plus<size_t>(),
                                     check_new_count);
  size_t layer_size_expected = new_states + 2;

  return with_state_type(dfa_state_count_is_wide(layer_size_expected), [&](auto curr_tag) -> DFAStateIdMap
  {
    typedef typename decltype(curr_tag)::type curr_id_t;

    MemoryMap<curr_id_t> curr_pairs_permutation_to_output(binary_dir() + "/pairs_permutation_to_output", curr_layer_count);

    auto check_new_id = [&](const hash_plus_index_t& curr_pair_hashed)
    {
      return curr_id_t(check_new(curr_pair_hashed));
    };

    TRY_PARALLEL_6(std::transform_inclusive_scan,
                   curr_transitions_hashed.begin(),
                   curr_transitions_hashed.end(),
                   curr_pairs_permutation_to_output.begin(),
                   [](curr_id_t previous, curr_id_t delta) {
                     curr_id_t output = previous + delta;
                     // check for overflow
                     assert(output >= previous);
                     return output;
                   },
                   check_new_id,
                   curr_id_t(1)); // first new state will be 2

    size_t layer_size = size_t(curr_pairs_permutation_to_output[curr_layer_count - 1]) + 1;
    assert(layer_size == layer_size_expected);

    profile.tic("unlink transitions_hashed");

    curr_transitions_hashed.unlink();

    profile.tic("states write");

    auto curr_pairs_permutation_to_output_begin = curr_pairs_permutation_to_output.begin();
    auto curr_pairs_permutation_to_output_end = curr_pairs_permutation_to_output.end();

    auto populate_transitions = [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
    {
      assert(new_state_id < layer_size);
      auto iter = std::lower_bound(curr_pairs_permutation_to_output_begin,
                                   curr_pairs_permutation_to_output_end,
                                   curr_id_t(new_state_id));
      assert(iter < curr_pairs_permutation_to_output_end);
      assert(*iter == new_state_id);

      size_t curr_pairs_permutation_index = iter - curr_pairs_permutation_to_output_begin;
      size_t curr_pair_rank = curr_pairs_permutation[curr_pairs_permutation_index];
      const next_id_t *row = &(curr_transitions[size_t(curr_pair_rank) * curr_layer_shape]);
      for(int j = 0; j < curr_layer_shape; ++j)
        {
          transitions_out[j] = dfa_state_t(row[j]);
        }
    };

    build_layer(layer, layer_size, populate_transitions);

    // Constant rows hold the id the scan gave their sorted position until
    // patched here, in rank order so their curr_transitions reads are
    // sequential.
    auto patch_constants = [&](curr_id_t *curr_pair_rank_to_output_data, size_t slice_begin, size_t slice_end)
    {
      TRY_PARALLEL_3(std::for_each,
                     curr_pair_rank_to_output_data + slice_begin,
                     curr_pair_rank_to_output_data + slice_end,
                     [&](curr_id_t& output)
                     {
                       size_t curr_pair_rank = size_t(&output - curr_pair_rank_to_output_data);
                       if(check_constant(curr_pair_rank))
                         {
                           output = curr_id_t(curr_transitions[curr_pair_rank * curr_layer_shape]);
                         }
                     });
    };

    MemoryMap<curr_id_t> curr_pair_rank_to_output = build_quadratic_write_pair_rank_to_output<curr_id_t, pair_rank_t>(layer,
                                                                                                             curr_layer_count,
                                                                                                             curr_pairs_permutation,
                                                                                                             curr_pairs_permutation_to_output,
                                                                                                             layer_size,
                                                                                                             state_bytes,
                                                                                                             std::to_string(key_bytes) + " keys",
                                                                                                             patch_constants,
                                                                                                             profile);

    // shrink state

    profile.tic("unlink pairs_permutation");

    curr_pairs_permutation.unlink();

    profile.tic("unlink transitions");

    curr_transitions.unlink();

    profile.tic("unlink pairs_permutation_to_output");

    curr_pairs_permutation_to_output.unlink();

    // cleanup in destructors
    profile.tic("cleanup");

    // done

    return DFAStateIdMap(std::move(curr_pair_rank_to_output));
  });
}

// Write pair_rank_to_output by scattering through the permutation:
// position i of the sorted order holds pair rank curr_pairs_permutation[i]
// and output id curr_pairs_permutation_to_output[i]. Reads are sequential
// and every write lands in a distinct slot, since the permutation is a
// bijection on pair ranks.
//
// The writes are random, so the output is split into slices of ranks and
// each slice gets its own pass over both inputs, writing only the ranks in
// that slice. A slice is kept to at most 1/8 of physical memory so it stays
// resident while the inputs stream past. With the whole output as one
// slice, cram_7x7 layer 42 (9.1e9 pairs, 36.5 GB of output next to 110 GB
// of streamed input) ran out of free memory, and the kernel wrote each
// output page back thousands of times: 35% done after 10 hours. Extra
// passes only cost sequential rereads of the inputs.
//
// Each pass:
// 1. Fills its slice with unwritten, the largest curr_id_t, which no
//    output id or constant can equal (ids are below layer_size, and
//    curr_id_t was picked so they stay at or below dfa_state_narrow_max).
// 2. Scatters the ranks in the slice.
// 3. Checks no slot in the slice still holds unwritten, which would
//    mean the permutation missed a rank. Across all slices, n writes
//    filling n slots also rules out duplicate ranks.
// 4. Calls patch_slice(output, slice_begin, slice_end), for a path whose
//    permutation_to_output does not yet hold constant rows' ids.
template <class curr_id_t, class pair_rank_t>
MemoryMap<curr_id_t> BinaryDFA::build_quadratic_write_pair_rank_to_output(int layer,
                                                                          size_t curr_layer_count,
                                                                          const MemoryMap<pair_rank_t>& curr_pairs_permutation,
                                                                          const MemoryMap<curr_id_t>& curr_pairs_permutation_to_output,
                                                                          size_t layer_size,
                                                                          const std::string& state_bytes,
                                                                          const std::string& key_description,
                                                                          const std::function<void(curr_id_t *, size_t, size_t)>& patch_slice,
                                                                          Profile& profile)
{
  const curr_id_t unwritten = std::numeric_limits<curr_id_t>::max();
  assert(uint64_t(layer_size - 1) < uint64_t(unwritten));

  profile.tic("output init");

  size_t output_slice_ranks_max = std::max(physical_memory_bytes() / 8 / sizeof(curr_id_t), size_t(1));
  size_t output_slices = std::max((curr_layer_count + output_slice_ranks_max - 1) / output_slice_ranks_max,
                                  binary_dfa_output_slices_min);
  size_t output_slice_ranks = (curr_layer_count + output_slices - 1) / output_slices;
  std::cout << "layer=" << layer << " pair_rank_to_output: " << output_slices << " slice(s) of up to " << output_slice_ranks << " ranks (" << (output_slice_ranks * sizeof(curr_id_t)) << " bytes each)"
            << "; state bytes: " << state_bytes << ", " << sizeof(curr_id_t) << " ids, " << key_description << std::endl;

  MemoryMap<curr_id_t> curr_pair_rank_to_output(binary_build_file_prefix(layer) + "-pair_rank_to_output", curr_layer_count);
  const pair_rank_t *curr_pairs_permutation_data = curr_pairs_permutation.begin();
  curr_id_t *curr_pair_rank_to_output_data = curr_pair_rank_to_output.begin();

  for(size_t slice = 0; slice < output_slices; ++slice)
    {
      size_t slice_begin = std::min(slice * output_slice_ranks, curr_layer_count);
      size_t slice_end = std::min(slice_begin + output_slice_ranks, curr_layer_count);
      std::cout << "layer=" << layer << " pair_rank_to_output slice " << (slice + 1) << "/" << output_slices << ": ranks [" << slice_begin << ", " << slice_end << ")" << std::endl;

      profile.tic("output slice fill");

      TRY_PARALLEL_3(std::fill,
                     curr_pair_rank_to_output_data + slice_begin,
                     curr_pair_rank_to_output_data + slice_end,
                     unwritten);

      profile.tic("output slice scatter");

      TRY_PARALLEL_3(std::for_each,
                     curr_pairs_permutation.begin(),
                     curr_pairs_permutation.end(),
                     [&](const pair_rank_t& curr_pair_rank_ref)
                     {
                       size_t curr_pair_rank = curr_pair_rank_ref;
                       assert(curr_pair_rank < curr_layer_count);
                       if((curr_pair_rank < slice_begin) || (slice_end <= curr_pair_rank))
                         {
                           return;
                         }
                       size_t curr_pairs_permutation_index = size_t(&curr_pair_rank_ref - curr_pairs_permutation_data);
                       curr_pair_rank_to_output_data[curr_pair_rank] = curr_pairs_permutation_to_output[curr_pairs_permutation_index];
                     });

      profile.tic("output slice check");

      curr_id_t *unwritten_slot = TRY_PARALLEL_3(std::find,
                                                 curr_pair_rank_to_output_data + slice_begin,
                                                 curr_pair_rank_to_output_data + slice_end,
                                                 unwritten);
      if(unwritten_slot != curr_pair_rank_to_output_data + slice_end)
        {
          throw std::logic_error("BinaryDFA: layer " + std::to_string(layer) + " pair rank " + std::to_string(unwritten_slot - curr_pair_rank_to_output_data) + " missing from pairs_permutation");
        }

      profile.tic("output slice constants");

      patch_slice(curr_pair_rank_to_output_data, slice_begin, slice_end);
    }

  return curr_pair_rank_to_output;
}

bool BinaryDFA::build_quadratic_pairs_wide(const DFA& left_in, const DFA& right_in, int layer)
{
  return (dfa_state_count_is_wide(left_in.get_layer_size(layer)) ||
          dfa_state_count_is_wide(right_in.get_layer_size(layer)));
}

size_t BinaryDFA::build_quadratic_pair_count(const DFA& left_in, const DFA& right_in, int layer)
{
  return with_state_type(build_quadratic_pairs_wide(left_in, right_in, layer), [&](auto tag)
  {
    typedef dfa_state_pair<typename decltype(tag)::type> pair_t;
    return build_quadratic_read_pairs<pair_t>(layer).size();
  });
}

int BinaryDFA::build_quadratic_forward(const DFA& left_in, const DFA& right_in)
{
  // this is the normal entry point for the forward pass.
  // it handles the special initialization of layer zero.

  // manual setup of initial layer

  dfa_state_t initial_left = left_in.get_initial_state();
  dfa_state_t initial_right = right_in.get_initial_state();

  with_state_type(build_quadratic_pairs_wide(left_in, right_in, 0), [&](auto tag)
  {
    typedef dfa_state_pair<typename decltype(tag)::type> pair_t;
    MemoryMap<pair_t> initial_pairs = memory_map_helper<pair_t>(0, "pairs", 1);
    initial_pairs[0] = pair_t(initial_left, initial_right);
  });

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
      profile.tic("layer=" + std::to_string(layer));

      size_t next_pairs_count = build_quadratic_forward_layer(left_in, right_in, layer);
      if(next_pairs_count == 0)
        {
          // current layer had pairs. next one did not.
          return layer + 1;
        }

      profile.tic("cleanup");
    }

  assert(layer_min == get_shape_size());
  return layer_min;
}

size_t BinaryDFA::build_quadratic_forward_layer(const DFA& left_in,
                                                const DFA& right_in,
                                                int layer)
{
  bool curr_pairs_wide = build_quadratic_pairs_wide(left_in, right_in, layer);
  bool next_pairs_wide = build_quadratic_pairs_wide(left_in, right_in, layer + 1);

  return with_state_type(curr_pairs_wide, [&](auto curr_tag)
  {
    typedef dfa_state_pair<typename decltype(curr_tag)::type> curr_pair_t;

    return with_state_type(next_pairs_wide, [&](auto next_tag)
    {
      typedef dfa_state_pair<typename decltype(next_tag)::type> next_pair_t;
      return build_quadratic_forward_layer<curr_pair_t, next_pair_t>(left_in, right_in, layer);
    });
  });
}

template <class curr_pair_t, class next_pair_t>
size_t BinaryDFA::build_quadratic_forward_layer(const DFA& left_in,
                                                const DFA& right_in,
                                                int layer)
{
  typedef typename next_pair_t::state_type next_state_t;

  Profile profile("build_quadratic_forward_layer");
  profile.set_prefix("layer=" + std::to_string(layer));

  profile.tic("init");

  // get size of current layer and next layer.
  //
  // using size_t for product calculations...

  size_t next_left_size = left_in.get_layer_size(layer + 1);
  size_t next_right_size = right_in.get_layer_size(layer + 1);

  // Filtering removes very few pairs here (0.04% at cram_7x7 layer 40), so
  // compacting for it would rewrite nearly the whole file to save almost
  // nothing. Instead each filtered pair is written as a sentinel that sorts
  // after every real pair, as the pairs are built. The sort moves the
  // sentinels to the end, and the unique after it copies the distinct real
  // pairs, about half of them, to the next layer's pairs file.

  // No real pair can equal this: next states are below the next layer's
  // size, and next_state_t was picked so they stay at or below
  // dfa_state_narrow_max, which is below the largest next_state_t.
  const next_state_t sentinel_state = std::numeric_limits<next_state_t>::max();
  assert(uint64_t(std::max(next_left_size, next_right_size) - 1) < uint64_t(sentinel_state));
  const next_pair_t sentinel(sentinel_state, sentinel_state);

  profile.tic("transitions");

  transition_pairs_marking<next_pair_t> marking;
  marking.filter_func = get_filter_func();
  marking.sentinel = sentinel;
  MemoryMap<next_pair_t> curr_transition_pairs = build_quadratic_transition_pairs<curr_pair_t, next_pair_t>(left_in, right_in, layer, &marking);

  std::cout << "pair count = " << curr_transition_pairs.size() << " (original, " << sizeof(next_state_t) << " bytes per state)" << std::endl;

  if(marking.found_sentinel)
    {
      throw std::logic_error("BinaryDFA: layer " + std::to_string(layer + 1) + " transition pairs include the sentinel pair");
    }

  auto working_begin = curr_transition_pairs.begin();
  auto working_end = curr_transition_pairs.end();
  size_t working_count = size_t(working_end - working_begin);
  size_t marked_count = marking.marked;
  std::cout << "pair count = " << (working_count - marked_count) << " (filtered, marked)" << std::endl;

  profile.tic("sort");

  // FlashSort orders pairs as operator< does, sentinels last, and throws
  // if the result is not sorted.
  FlashSort<next_pair_t>().sort(working_begin, working_end);

  profile.tic("post-unique");

  working_end = std::lower_bound(working_begin, working_end, sentinel);
  if(size_t(working_end - working_begin) != working_count - marked_count)
    {
      throw std::logic_error("BinaryDFA: layer " + std::to_string(layer + 1) + " has " + std::to_string(working_end - working_begin) + " pairs before the sentinels after sorting, expected " + std::to_string(working_count - marked_count));
    }
  size_t sorted_count = size_t(working_end - working_begin);

  // The distinct pairs are copied to a new file in parallel blocks: each
  // block counts the pairs that differ from the one before them, a prefix
  // sum of the counts gives each block's output offset, and each block then
  // writes its pairs there. Both passes only read the sorted pairs.
  size_t unique_block_size = std::max(sorted_count / (8 * std::max(size_t(std::thread::hardware_concurrency()), size_t(1))), size_t(1) << 16);
  size_t unique_block_count = (sorted_count + unique_block_size - 1) / unique_block_size;
  std::vector<size_t> unique_blocks(unique_block_count);
  std::iota(unique_blocks.begin(), unique_blocks.end(), size_t(0));

  auto is_distinct = [&](size_t i)
  {
    return (i == 0) || (working_begin[i] != working_begin[i - 1]);
  };

  std::vector<size_t> unique_offsets(unique_block_count + 1, 0);
  auto count_block = [&](size_t block)
  {
    size_t i_begin = block * unique_block_size;
    size_t i_end = std::min(i_begin + unique_block_size, sorted_count);
    size_t distinct = 0;
    for(size_t i = i_begin; i < i_end; ++i)
      {
        distinct += is_distinct(i);
      }
    unique_offsets[block + 1] = distinct;
  };
  TRY_PARALLEL_3(std::for_each, unique_blocks.begin(), unique_blocks.end(), count_block);
  std::partial_sum(unique_offsets.begin(), unique_offsets.end(), unique_offsets.begin());
  size_t next_pairs_count = unique_offsets.back();

  profile.tic("unique write");

  MemoryMap<next_pair_t> next_pairs(binary_dir() + "/next_pairs_unique", next_pairs_count);

  // Each block checks that it writes exactly its counted pairs, each greater
  // than the one it wrote before.
  std::atomic<bool> unique_write_failed(false);
  auto write_block = [&](size_t block)
  {
    size_t i_begin = block * unique_block_size;
    size_t i_end = std::min(i_begin + unique_block_size, sorted_count);
    size_t o = unique_offsets[block];
    size_t o_end = unique_offsets[block + 1];
    for(size_t i = i_begin; i < i_end; ++i)
      {
        if(is_distinct(i))
          {
            if((o >= o_end) || ((o > unique_offsets[block]) && !(next_pairs[o - 1] < working_begin[i])))
              {
                unique_write_failed = true;
                return;
              }
            next_pairs[o++] = working_begin[i];
          }
      }
    if(o != o_end)
      {
        unique_write_failed = true;
      }
  };
  TRY_PARALLEL_3(std::for_each, unique_blocks.begin(), unique_blocks.end(), write_block);
  if(unique_write_failed)
    {
      throw std::logic_error("BinaryDFA: layer " + std::to_string(layer + 1) + " unique pairs did not match their block counts or were not increasing");
    }

  // pairs across each block boundary increase too
  for(size_t block = 1; block < unique_block_count; ++block)
    {
      size_t o = unique_offsets[block];
      if((o > 0) && (o < next_pairs_count) && !(next_pairs[o - 1] < next_pairs[o]))
        {
          throw std::logic_error("BinaryDFA: layer " + std::to_string(layer + 1) + " unique pairs do not increase at offset " + std::to_string(o));
        }
    }

  std::cout << "pair count = " << next_pairs_count << " (post sort unique)" << std::endl;

  // the sorted pairs are dropped without flushing them, then the next pairs
  // are flushed, and renamed into place so the next pairs update is atomic
  // and restarts are easier.

  profile.tic("unlink sorted");

  curr_transition_pairs.unlink();

  profile.tic("munmap");

  next_pairs.munmap();

  profile.tic("stats");

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
  next_pairs.rename(next_pairs_name);

#ifdef PARANOIA
  profile.tic("next pairs paranoia");

  next_pairs.mmap();
  assert(TRY_PARALLEL_2(std::is_sorted, next_pairs.begin(), next_pairs.end()));
#endif

  profile.tic("done");

  return next_pairs_count;
}

template <class curr_pair_t, class next_pair_t>
MemoryMap<next_pair_t> BinaryDFA::build_quadratic_transition_pairs(const DFA& left_in,
                                                                   const DFA& right_in,
                                                                   int layer,
                                                                   transition_pairs_marking<next_pair_t> *marking)
{
  typedef typename next_pair_t::state_type next_state_t;

  Profile profile2("build_quadratic_transition_pairs");

  profile2.tic("init");

  int curr_layer_shape = this->get_layer_shape(layer);
  const MemoryMap<curr_pair_t> curr_pairs = build_quadratic_read_pairs<curr_pair_t>(layer);
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
  MemoryMap<next_state_t> transition_pairs_left(binary_dir() + "/transition_pairs_left", transition_pairs_size, [&](size_t transition_index)
  {
    size_t curr_i = transition_index / curr_layer_shape;
    size_t curr_j = transition_index % curr_layer_shape;

    curr_pair_t curr_pair = curr_pairs[curr_i];
    dfa_state_t curr_left_state = curr_pair.get_left_state();
    assert(curr_left_state < curr_left_size);

    DFATransitionsReference left_transitions = left_in.get_transitions(layer, curr_left_state);
    dfa_state_t next_left_state = left_transitions.at(curr_j);
    assert(std::in_range<next_state_t>(next_left_state));
    return next_state_t(next_left_state);
  });

  // read right transitions
  profile2.tic("right");

  right_in.get_transitions(layer, 0);

  MemoryMap<next_pair_t> curr_transition_pairs(binary_dir() + "/transition_pairs", transition_pairs_size, [&](size_t transition_index)
  {
    size_t curr_i = transition_index / curr_layer_shape;
    size_t curr_j = transition_index % curr_layer_shape;

    curr_pair_t curr_pair = curr_pairs[curr_i];
    dfa_state_t curr_right_state = curr_pair.get_right_state();
    assert(curr_right_state < curr_right_size);

    DFATransitionsReference right_transitions = right_in.get_transitions(layer, curr_right_state);
    next_pair_t next_pair(transition_pairs_left[transition_index], right_transitions.at(curr_j));
    if(marking)
      {
        if(next_pair == marking->sentinel)
          {
            marking->found_sentinel = true;
          }
        if(marking->filter_func(next_pair.get_left_state(), next_pair.get_right_state()))
          {
            ++marking->marked;
            return marking->sentinel;
          }
      }
    return next_pair;
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
