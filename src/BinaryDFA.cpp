// BinaryDFA.cpp

#include "BinaryDFA.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <execution>
#include <iostream>
#include <memory>
#include <numeric>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <queue>
#include <ranges>
#include <sstream>
#include <string>
#include <unistd.h>

#include "Flashsort.h"
#include "MemoryMap.h"
#include "Profile.h"
#include "ScratchConfig.h"
#include "VectorBitSet.h"
#include "parallel.h"

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

dfa_state_t BinaryDFA::build_quadratic_backward(const DFA& left_in,
                                                const DFA& right_in,
                                                int backward_layers)
{
  Profile profile("build_quadratic_backward");

  // initially empty since shortcircuiting will happen by last layer.

  MemoryMap<dfa_state_t> next_pair_rank_to_output(1); // dummy initialization

  // backward pass

  for(int layer = backward_layers - 1; layer >= 0; --layer)
    {
      profile.tic("layer=" + std::to_string(layer));

      MemoryMap<dfa_state_t> curr_pair_rank_to_output =
        build_quadratic_backward_layer(left_in,
                                       right_in,
                                       layer,
                                       next_pair_rank_to_output);

      next_pair_rank_to_output.unlink();
      std::swap(curr_pair_rank_to_output, next_pair_rank_to_output);
    }

  assert(next_pair_rank_to_output.size() == 1);
  dfa_state_t initial_state = next_pair_rank_to_output[0];
  next_pair_rank_to_output.unlink();
  return initial_state;
}

MemoryMap<dfa_state_t> BinaryDFA::build_quadratic_backward_layer(const DFA& left_in,
                                                                 const DFA& right_in,
                                                                 int layer,
                                                                 const MemoryMap<dfa_state_t>& next_pair_rank_to_output)
{
  Profile profile("build_quadratic_backward_layer");

  profile.set_prefix("layer=" + std::to_string(layer));
  profile.tic("init");

  int curr_layer_shape = this->get_layer_shape(layer);
  const MemoryMap<dfa_state_pair_t> curr_pairs = build_quadratic_read_pairs(layer);
  size_t curr_layer_count = curr_pairs.size();
  // not long term necessary, but guarantees that final DFA will fit within current limit.
  assert(curr_layer_count <= DFA_STATE_MAX);

  size_t next_left_size = left_in.get_layer_size(layer + 1);
  size_t next_right_size = right_in.get_layer_size(layer + 1);

  profile.tic("next pair mmap");

  const MemoryMap<dfa_state_pair_t> next_pairs = build_quadratic_read_pairs(layer + 1);
  size_t next_layer_count = next_pairs.size();

  profile.tic("next pair index");

  // index entries have first pair of 4KB block (64 bit pair)
  std::vector<MemoryMap<dfa_state_pair_t>> next_pairs_index;
  next_pairs_index.reserve(3);
  auto add_next_pairs_index = [&](const MemoryMap<dfa_state_pair_t>& previous_pairs)
  {
    std::string index_name = binary_dir() + "/next_pairs_index-" + std::to_string(next_pairs_index.size());
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

  profile.tic("transitions input");

  MemoryMap<dfa_state_pair_t> curr_transition_pairs = build_quadratic_transition_pairs(left_in, right_in, layer);

  profile.tic("transitions populate");

  auto filter_func = get_filter_func();
  auto shortcircuit_func = get_shortcircuit_func();

  MemoryMap<dfa_state_t> curr_transitions(binary_dir() + "/transitions", curr_layer_count * curr_layer_shape, [&](size_t next_pair_index)
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

        for(size_t index_index = next_pairs_index.size() - 1; index_index > 0; --index_index)
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

  profile.tic("transitions hash");

  // Below the threshold the sort key is the transitions themselves, so the
  // sort leaves states in canonical order (FORMAT-DFA.md section 8) and the
  // scan below numbers them 2, 3, ... in that order. Above it the key is a
  // hash and the order is arbitrary.
  if(curr_layer_shape + 1 > binary_dfa_hash_width)
    {
      hashed_any_layer = true;
    }

  MemoryMap<BinaryDFATransitionsHashPlusIndex> curr_transitions_hashed(binary_dir() + "/transitions_hashed", curr_layer_count, [&](size_t i)
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

    assert(i <= DFA_STATE_MAX);
    output.data[binary_dfa_hash_width - 1] = dfa_state_t(i);

    return output;
  });

  profile.tic("sort hash");

  TRY_PARALLEL_2(std::sort, curr_transitions_hashed.begin(), curr_transitions_hashed.end());

  profile.tic("sort hash check");

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

  profile.tic("sort permutation");

  MemoryMap<dfa_state_t> curr_pairs_permutation(binary_dir() + "/pairs_permutation", curr_layer_count, [&](size_t i)
  {
    return curr_transitions_hashed[i].get_pair_rank();
  });

  profile.tic("states identification");

  MemoryMap<dfa_state_t> curr_pairs_permutation_to_output(binary_dir() + "/pairs_permutation_to_output", curr_layer_count);

  auto check_constant = [&](size_t curr_pair_rank)
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
                 [](dfa_state_t previous, dfa_state_t delta) {
                   dfa_state_t output = previous + delta;
                   // check for overflow
                   assert(output >= previous);
                   return output;
                 },
                 check_new,
                 1); // first new state will be 2

  dfa_state_t layer_size = curr_pairs_permutation_to_output[curr_layer_count - 1] + 1;

  profile.tic("unlink transitions_hashed");

  curr_transitions_hashed.unlink();

  profile.tic("states write");

  auto curr_pairs_permutation_to_output_begin = curr_pairs_permutation_to_output.begin();
  auto curr_pairs_permutation_to_output_end = curr_pairs_permutation_to_output.end();
  auto curr_transitions_begin = curr_transitions.begin();

  auto populate_transitions = [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
  {
    assert(new_state_id < layer_size);
    auto iter = std::lower_bound(curr_pairs_permutation_to_output_begin,
                                 curr_pairs_permutation_to_output_end,
                                 new_state_id);
    assert(iter < curr_pairs_permutation_to_output_end);
    assert(*iter == new_state_id);

    size_t curr_pairs_permutation_index = iter - curr_pairs_permutation_to_output_begin;
    dfa_state_t curr_pair_rank = curr_pairs_permutation[curr_pairs_permutation_index];
    std::copy_n(curr_transitions_begin + size_t(curr_pair_rank) * curr_layer_shape, curr_layer_shape, transitions_out);
  };

  build_layer(layer, layer_size, populate_transitions);

  profile.tic("invert");

  // invert permutation so we can write pair_rank_to_output in order

  MemoryMap<dfa_state_t> curr_pairs_permutation_inverse(binary_dir() + "/pairs_permutation_inverse", curr_layer_count);
  // contents are indexes into curr_pairs_permutation
  std::iota(curr_pairs_permutation_inverse.begin(),
            curr_pairs_permutation_inverse.end(),
            0);
  // sort so curr_pairs_permutation[curr_pairs_permutation_inverse[i]] = i

  TRY_PARALLEL_3(std::sort,
                 curr_pairs_permutation_inverse.begin(),
                 curr_pairs_permutation_inverse.end(),
                 [&](dfa_state_t a, dfa_state_t b) {
                   return curr_pairs_permutation[a] < curr_pairs_permutation[b];
                 });

  for(size_t i : {size_t(0), curr_layer_count - 1})
    {
      assert(curr_pairs_permutation[curr_pairs_permutation_inverse[i]] == i);
    }

  profile.tic("unlink pairs_permutation");

  curr_pairs_permutation.unlink();

  profile.tic("output");

  MemoryMap<dfa_state_t> curr_pair_rank_to_output(binary_build_file_prefix(layer) + "-pair_rank_to_output", curr_layer_count, [&](size_t curr_pair_rank)
  {
    dfa_state_t curr_pairs_permutation_index = curr_pairs_permutation_inverse[curr_pair_rank];
    if(check_constant(curr_pair_rank))
      {
        return curr_transitions[size_t(curr_pair_rank) * curr_layer_shape];
      }

    return curr_pairs_permutation_to_output[curr_pairs_permutation_index];
  });

  assert(curr_pair_rank_to_output.size() > 0);

  // shrink state

  profile.tic("unlink transitions");

  curr_transitions.unlink();

  profile.tic("unlink pairs_permutation_inverse");

  curr_pairs_permutation_inverse.unlink();

  profile.tic("unlink pairs_permutation_to_output");

  curr_pairs_permutation_to_output.unlink();

  // cleanup in destructors
  profile.tic("cleanup");

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
      profile.tic("layer=" + std::to_string(layer));

      MemoryMap<dfa_state_pair_t> next_pairs = build_quadratic_forward_layer(left_in, right_in, layer);
      if(next_pairs.size() == 0)
        {
          // current layer had pairs. next one did not.
          return layer + 1;
        }

#ifdef PARANOIA
      profile.tic("next pairs paranoia");

      assert(TRY_PARALLEL_2(std::is_sorted, next_pairs.begin(), next_pairs.end()));
#endif

      profile.tic("cleanup");
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

  std::cout << "pair count = " << (working_end - working_begin) << " (filtered)" << std::endl;

  profile.tic("pre-unique");

  working_end = TRY_PARALLEL_2(std::unique,
                               working_begin,
                               working_end);

  std::cout << "pair count = " << (working_end - working_begin) << " (pre sort unique)" << std::endl;

  profile.tic("sort");

  TRY_PARALLEL_2(std::sort,
                 working_begin,
                 working_end);

  profile.tic("post-unique");

  working_end = TRY_PARALLEL_2(std::unique,
                               working_begin,
                               working_end);

  std::cout << "pair count = " << (working_end - working_begin) << " (post sort unique)" << std::endl;

  // the following truncate and rename to the next pairs file are to
  // make the next pairs updates atomic and make restarts easier.

  profile.tic("munmap");

  // this munmap is implied by the truncate, but separating to track
  // the timing.
  curr_transition_pairs.munmap();

  profile.tic("truncate");

  size_t next_pairs_count = working_end - working_begin;
  curr_transition_pairs.truncate(next_pairs_count);

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
  MemoryMap<dfa_state_t> transition_pairs_left(binary_dir() + "/transition_pairs_left", transition_pairs_size, [&](size_t transition_index)
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

  MemoryMap<dfa_state_pair_t> curr_transition_pairs(binary_dir() + "/transition_pairs", transition_pairs_size, [&](size_t transition_index)
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

////////////////////////////////////////////////////////////////////////////
// n-ary union/intersection construction.
//
// This generalizes the pairwise machinery above from dfa_state_pair_t
// (a fixed 8-byte struct, one state per side) to a "tuple": a row of
// dfas_in.size() dfa_state_t values, one per input DFA, stored flat --
// tuple i's values live at [i * width, (i + 1) * width) of whichever
// MemoryMap<dfa_state_t> holds them. width varies per call, so tuples
// cannot be a fixed C++ type the way dfa_state_pair_t is; every place
// below that would have taken a dfa_state_pair_t instead takes a raw
// pointer plus width, or an index into a flat array.
//
// The forward and backward passes otherwise mirror build_quadratic_forward
// and build_quadratic_backward exactly: forward finds every tuple
// reachable from the all-initial-states tuple, layer by layer, dropping
// (via nary_filter) any tuple that already shortcircuits to 0 or 1;
// backward walks layers in reverse, turning each layer's surviving tuples
// into actual DFA states.
//
// Canonical numbering (FORMAT-DFA.md section 8) sorts each layer's *output*
// rows -- width curr_layer_shape, the alphabet size at that board square,
// which does not depend on dfas_in.size() at all -- directly, by their own
// values, rather than through build_quadratic's hashed proxy key. That
// key exists there purely to keep the sort comparison to a fixed 16 bytes
// when curr_layer_shape is large (e.g. chess's per-square or 64-wide
// layers); it is a speed optimization, not a correctness requirement, and
// dropping it here means every layer stays canonical -- see
// build_nary_backward_layer. The n-wide *tuple* bookkeeping (forward
// dedup, backward tuple-to-rank lookup) still uses a plain direct
// comparison rather than a hashed key; for large dfas_in.size() that is a
// slower sort than build_quadratic's analogous trick would give, a known
// simplification rather than a correctness gap.

bool BinaryDFA::nary_filter(const dfa_state_t *tuple, size_t width) const
{
  // True iff every element of the tuple can be resolved to a state in the
  // terminal pseudo-layer without looking at the next layer: either every
  // element is already 0 or 1, or at least one equals this function's sink
  // (1 for union, 0 for intersection) and so decides the whole tuple by
  // itself.

  dfa_state_t sink = nary_is_union ? 1 : 0;

  bool all_constant = true;
  for(size_t i = 0; i < width; ++i)
    {
      dfa_state_t s = tuple[i];
      if(s == sink)
        {
          return true;
        }
      if(s >= 2)
        {
          all_constant = false;
        }
    }

  return all_constant;
}

dfa_state_t BinaryDFA::nary_shortcircuit(const dfa_state_t *tuple, size_t width) const
{
  // Valid only where nary_filter(tuple, width) is true.

  dfa_state_t sink = nary_is_union ? 1 : 0;

  for(size_t i = 0; i < width; ++i)
    {
      if(tuple[i] == sink)
        {
          return sink;
        }
    }

  // nary_filter guarantees that if no element is the sink, every element is
  // constant -- so every element is the *other* reserved state.
  return nary_is_union ? dfa_state_t(0) : dfa_state_t(1);
}

BinaryDFA::BinaryDFA(const std::vector<shared_dfa_ptr>& dfas_in, bool is_union_in)
  : BinaryDFA(dfas_in.at(0)->get_shape(),
              BinaryFunction(is_union_in
                             ? std::function<bool(bool, bool)>([](bool l, bool r) {return l || r;})
                             : std::function<bool(bool, bool)>([](bool l, bool r) {return l && r;})))
{
  // dfas_in.at(0) above both gives the shape and fails loudly on an empty
  // vector, rather than silently building a DFA over no inputs.

  nary_is_union = is_union_in;

  for(const shared_dfa_ptr& dfa : dfas_in)
    {
      assert(dfa.get());
      assert(dfa->get_shape() == this->get_shape());
      dfa->mmap();
    }

  std::vector<dfa_state_t> initial_tuple(dfas_in.size());
  for(size_t i = 0; i < dfas_in.size(); ++i)
    {
      initial_tuple[i] = dfas_in[i]->get_initial_state();
    }

  if(nary_filter(initial_tuple.data(), initial_tuple.size()))
    {
      // No ordinary states, so canonical numbering is vacuous.
      this->set_canonical(true);
      this->set_initial_state(nary_shortcircuit(initial_tuple.data(), initial_tuple.size()));
      return;
    }

  build_nary(dfas_in);
}

void BinaryDFA::build_nary(const std::vector<shared_dfa_ptr>& dfas_in)
{
  Profile profile("build_nary");
  DirectoryGuard binary_directory_guard = create_binary_directory();

  profile.tic("forward");

  int num_layers_used = build_nary_forward(dfas_in);

  profile.tic("backward");

  dfa_state_t initial_state = build_nary_backward(dfas_in, num_layers_used);

  // Always canonical -- see the design note above build_nary_backward_layer.
  this->set_canonical(true);
  this->set_initial_state(initial_state);

  assert(this->ready());
  profile.tic("final cleanup");
}

std::string BinaryDFA::nary_tuples_name(int layer) const
{
  return binary_build_file_prefix(layer) + "-tuples";
}

MemoryMap<dfa_state_t> BinaryDFA::build_nary_read_tuples(int layer) const
{
  return MemoryMap<dfa_state_t>(nary_tuples_name(layer));
}

int BinaryDFA::build_nary_forward(const std::vector<shared_dfa_ptr>& dfas_in)
{
  Profile profile("build_nary_forward");

  size_t width = dfas_in.size();

  profile.tic("initial tuple");

  MemoryMap<dfa_state_t> initial_tuple(nary_tuples_name(0), width, [&](size_t i)
  {
    return dfas_in[i]->get_initial_state();
  });

  for(int layer = 0; layer < get_shape_size(); ++layer)
    {
      profile.tic("layer=" + std::to_string(layer));

      MemoryMap<dfa_state_t> next_tuples = build_nary_forward_layer(dfas_in, layer);
      if(next_tuples.size() == 0)
        {
          // current layer had tuples, next one did not -- every survivor
          // shortcircuited, so there is nothing left to track past here.
          return layer + 1;
        }
    }

  return get_shape_size();
}

MemoryMap<dfa_state_t> BinaryDFA::build_nary_forward_layer(const std::vector<shared_dfa_ptr>& dfas_in, int layer)
{
  Profile profile("build_nary_forward_layer");
  profile.set_prefix("layer=" + std::to_string(layer));

  profile.tic("transitions");

  size_t width = dfas_in.size();

  MemoryMap<dfa_state_t> curr_transition_tuples = build_nary_transition_tuples(dfas_in, layer);
  size_t raw_row_count = curr_transition_tuples.size() / width;

  auto curr_begin = curr_transition_tuples.begin();
  auto row_ptr = [=](size_t row) { return curr_begin + row * width; };

  profile.tic("row order init");

  MemoryMap<size_t> row_order(binary_dir() + "/forward_row_order-" + std::to_string(layer), raw_row_count, [](size_t i)
  {
    return i;
  });

  profile.tic("filter");

  auto working_begin = row_order.begin();
  auto working_end = row_order.end();

  working_end = TRY_PARALLEL_3(std::remove_if, working_begin, working_end, [&](size_t row)
  {
    return nary_filter(row_ptr(row), width);
  });

  profile.tic("sort");

  TRY_PARALLEL_3(std::sort, working_begin, working_end, [&](size_t a, size_t b)
  {
    const dfa_state_t *pa = row_ptr(a);
    const dfa_state_t *pb = row_ptr(b);
    return std::lexicographical_compare(pa, pa + width, pb, pb + width);
  });

  profile.tic("unique");

  working_end = TRY_PARALLEL_3(std::unique, working_begin, working_end, [&](size_t a, size_t b)
  {
    return std::equal(row_ptr(a), row_ptr(a) + width, row_ptr(b));
  });

  size_t next_count = size_t(working_end - working_begin);

  profile.tic("gather");

  MemoryMap<dfa_state_t> next_tuples(nary_tuples_name(layer + 1), next_count * width, [&](size_t idx)
  {
    size_t out_row = idx / width;
    size_t j = idx % width;
    size_t src_row = row_order[out_row];
    return curr_transition_tuples[src_row * width + j];
  });

  profile.tic("unlink");

  curr_transition_tuples.unlink();
  row_order.unlink();

  profile.tic("done");

  return next_tuples;
}

MemoryMap<dfa_state_t> BinaryDFA::build_nary_transition_tuples(const std::vector<shared_dfa_ptr>& dfas_in, int layer)
{
  Profile profile("build_nary_transition_tuples");

  profile.tic("init");

  size_t width = dfas_in.size();
  int curr_layer_shape = this->get_layer_shape(layer);

  const MemoryMap<dfa_state_t> curr_tuples = build_nary_read_tuples(layer);
  size_t curr_row_count = curr_tuples.size() / width;
  assert(curr_row_count > 0);

  // make sure inputs are memory mapped before going parallel
  curr_tuples.mmap();
  for(size_t i = 0; i < width; ++i)
    {
      dfas_in[i]->get_transitions(layer, 0);
    }

  profile.tic("transitions");

  size_t output_size = curr_row_count * size_t(curr_layer_shape) * width;
  MemoryMap<dfa_state_t> output(binary_dir() + "/transition_tuples-" + std::to_string(layer), output_size, [&](size_t idx) -> dfa_state_t
  {
    size_t j = idx % width;
    size_t rc = idx / width;
    size_t c = rc % size_t(curr_layer_shape);
    size_t row = rc / size_t(curr_layer_shape);

    dfa_state_t curr_state = curr_tuples[row * width + j];
    DFATransitionsReference transitions = dfas_in[j]->get_transitions(layer, curr_state);
    return transitions.at(c);
  });

  profile.tic("done");

  return output;
}

dfa_state_t BinaryDFA::build_nary_backward(const std::vector<shared_dfa_ptr>& dfas_in, int backward_layers)
{
  Profile profile("build_nary_backward");

  // initially empty since shortcircuiting will happen by last layer.

  MemoryMap<dfa_state_t> next_rank_to_output(1); // dummy initialization

  for(int layer = backward_layers - 1; layer >= 0; --layer)
    {
      profile.tic("layer=" + std::to_string(layer));

      MemoryMap<dfa_state_t> curr_rank_to_output = build_nary_backward_layer(dfas_in, layer, next_rank_to_output);

      next_rank_to_output.unlink();
      std::swap(curr_rank_to_output, next_rank_to_output);
    }

  assert(next_rank_to_output.size() == 1);
  dfa_state_t initial_state = next_rank_to_output[0];
  next_rank_to_output.unlink();

  // tuples(0) (the initial tuple, written before the forward loop even
  // starts) is read as curr_tuples during the layer=0 iteration above, but
  // it is never anyone's next_tuples -- there is no layer -1 -- so
  // build_nary_backward_layer's own cleanup never reaches it. Free it here
  // instead of leaving it for binary_directory_guard's final teardown.
  build_nary_read_tuples(0).unlink();

  return initial_state;
}

MemoryMap<dfa_state_t> BinaryDFA::build_nary_backward_layer(const std::vector<shared_dfa_ptr>& dfas_in,
                                                             int layer,
                                                             const MemoryMap<dfa_state_t>& next_rank_to_output)
{
  Profile profile("build_nary_backward_layer");
  profile.set_prefix("layer=" + std::to_string(layer));

  profile.tic("init");

  size_t width = dfas_in.size();
  int curr_layer_shape = this->get_layer_shape(layer);

  profile.tic("next tuples");

  const MemoryMap<dfa_state_t> next_tuples = build_nary_read_tuples(layer + 1);
  size_t next_layer_count = next_tuples.size() / width;

  // next_tuples.begin() asserts the map is actually mmap'd, which a
  // zero-length MemoryMap never is (MemoryMap::mmap() short circuits for
  // length 0) -- so this must stay lazy, evaluated only from inside
  // find_next_rank, which by construction is only ever called when
  // next_layer_count > 0 (see build_nary_forward's early-return comment:
  // the layer whose *next* tuples file is empty is exactly the layer where
  // nary_filter is true for every transition, so find_next_rank is never
  // reached there).
  auto next_row_ptr = [&](size_t row) { return next_tuples.begin() + row * width; };

  auto find_next_rank = [&](const dfa_state_t *tuple) -> size_t
  {
    // binary search for tuple within next_tuples, which is sorted ascending
    // (build_nary_forward_layer's sort step).
    size_t lo = 0;
    size_t hi = next_layer_count;
    while(lo < hi)
      {
        size_t mid = lo + (hi - lo) / 2;
        const dfa_state_t *mid_row = next_row_ptr(mid);
        if(std::lexicographical_compare(mid_row, mid_row + width, tuple, tuple + width))
          {
            lo = mid + 1;
          }
        else
          {
            hi = mid;
          }
      }
    return lo;
  };

  profile.tic("transitions input");

  MemoryMap<dfa_state_t> curr_transition_tuples = build_nary_transition_tuples(dfas_in, layer);
  size_t curr_layer_count = curr_transition_tuples.size() / (size_t(curr_layer_shape) * width);
  // not long term necessary, but guarantees that final DFA will fit within current limit.
  assert(curr_layer_count > 0);
  assert(curr_layer_count <= DFA_STATE_MAX);

  profile.tic("transitions populate");

  auto curr_transition_tuples_begin = curr_transition_tuples.begin();

  MemoryMap<dfa_state_t> curr_transitions(binary_dir() + "/transitions-" + std::to_string(layer), curr_layer_count * size_t(curr_layer_shape), [&](size_t idx) -> dfa_state_t
  {
    const dfa_state_t *tuple = curr_transition_tuples_begin + idx * width;

    if(nary_filter(tuple, width))
      {
        return nary_shortcircuit(tuple, width);
      }

    size_t rank = find_next_rank(tuple);
    assert(rank < next_layer_count);
    assert(std::equal(next_row_ptr(rank), next_row_ptr(rank) + width, tuple));

    return next_rank_to_output[rank];
  });

  profile.tic("unlink transition_tuples");

  curr_transition_tuples.unlink();

  // next_tuples (nary_tuples_name(layer + 1)) has no more readers. The only
  // other place any tuples file gets read is as curr_tuples, inside
  // build_nary_transition_tuples, one call to this function per layer; the
  // backward loop runs from backward_layers - 1 down to 0, so tuples(layer
  // + 1) was already consumed that way during the previous (higher-layer)
  // call and is not needed again. Without this, every layer's tuples file
  // -- one per layer, all written by the forward pass before backward ever
  // starts freeing any -- would sit in binarydfa/<pid> for the entire
  // backward pass instead of shrinking as it goes.
  next_tuples.unlink();

  // Below, sort curr_transitions's rows -- each of width curr_layer_shape,
  // this layer's own alphabet size, unrelated to dfas_in.size() -- directly
  // by their own values, never through a hashed proxy key. That is what
  // makes this constructor's canonical claim unconditional: states come out
  // numbered in the ascending order FORMAT-DFA.md section 8 requires, with
  // no build_quadratic-style hashed_any_layer escape hatch to give it up.

  profile.tic("row order init");

  MemoryMap<size_t> row_order(binary_dir() + "/backward_row_order-" + std::to_string(layer), curr_layer_count, [](size_t i)
  {
    return i;
  });

  auto curr_transitions_begin = curr_transitions.begin();
  auto out_row_ptr = [=](size_t row) { return curr_transitions_begin + row * size_t(curr_layer_shape); };

  auto check_constant = [&](size_t row) -> bool
  {
    const dfa_state_t *r = out_row_ptr(row);
    dfa_state_t first = r[0];
    if(first >= 2)
      {
        return false;
      }
    for(int j = 1; j < curr_layer_shape; ++j)
      {
        if(r[j] != first)
          {
            return false;
          }
      }
    return true;
  };

  profile.tic("sort");

  TRY_PARALLEL_3(std::sort, row_order.begin(), row_order.end(), [&](size_t a, size_t b)
  {
    const dfa_state_t *pa = out_row_ptr(a);
    const dfa_state_t *pb = out_row_ptr(b);
    return std::lexicographical_compare(pa, pa + curr_layer_shape, pb, pb + curr_layer_shape);
  });

  profile.tic("states identification");

  MemoryMap<dfa_state_t> sorted_index_to_output_id(binary_dir() + "/backward_scan-" + std::to_string(layer), curr_layer_count);

  auto row_order_begin = row_order.begin();

  auto check_new = [&](const size_t& row_ref) -> dfa_state_t
  {
    size_t curr_row = row_ref;
    if(check_constant(curr_row))
      {
        return 0;
      }

    if(&row_ref <= row_order_begin)
      {
        // first and non-constant is always a new state
        // TODO : change range to not require this
        return 1;
      }

    size_t prev_row = (&row_ref)[-1];
    if(std::equal(out_row_ptr(prev_row), out_row_ptr(prev_row) + curr_layer_shape, out_row_ptr(curr_row)))
      {
        // same transitions as predecessor (which may itself be constant --
        // impossible here, since curr_row just failed check_constant and a
        // constant row can never equal a non-constant one)
        return 0;
      }

    return 1;
  };

  TRY_PARALLEL_6(std::transform_inclusive_scan,
                 row_order.begin(),
                 row_order.end(),
                 sorted_index_to_output_id.begin(),
                 [](dfa_state_t previous, dfa_state_t delta) {
                   dfa_state_t output = previous + delta;
                   // check for overflow
                   assert(output >= previous);
                   return output;
                 },
                 check_new,
                 1); // first new state will be 2

  dfa_state_t layer_size = sorted_index_to_output_id[curr_layer_count - 1] + 1;

  profile.tic("states write");

  auto sorted_index_to_output_id_begin = sorted_index_to_output_id.begin();
  auto sorted_index_to_output_id_end = sorted_index_to_output_id.end();

  auto populate_transitions = [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
  {
    assert(new_state_id >= 2);
    assert(new_state_id < layer_size);

    auto iter = std::lower_bound(sorted_index_to_output_id_begin, sorted_index_to_output_id_end, new_state_id);
    assert(iter < sorted_index_to_output_id_end);
    assert(*iter == new_state_id);

    size_t sorted_position = size_t(iter - sorted_index_to_output_id_begin);
    size_t row = row_order[sorted_position];
    std::copy_n(out_row_ptr(row), curr_layer_shape, transitions_out);
  };

  build_layer(layer, layer_size, populate_transitions);

  profile.tic("output");

  MemoryMap<dfa_state_t> curr_rank_to_output(binary_build_file_prefix(layer) + "-rank_to_output", curr_layer_count);

  TRY_PARALLEL_3(std::for_each, row_order.begin(), row_order.end(), [&](const size_t& row_ref)
  {
    size_t sorted_position = size_t(&row_ref - row_order_begin);
    size_t row = row_ref;

    curr_rank_to_output[row] = check_constant(row)
      ? curr_transitions[row * size_t(curr_layer_shape)]
      : sorted_index_to_output_id[sorted_position];
  });

  assert(curr_rank_to_output.size() > 0);

  profile.tic("unlink");

  curr_transitions.unlink();
  row_order.unlink();
  sorted_index_to_output_id.unlink();

  profile.tic("cleanup");

  return curr_rank_to_output;
}
