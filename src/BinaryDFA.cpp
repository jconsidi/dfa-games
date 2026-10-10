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

size_t binary_dfa_narrow_pair_count_max = UINT32_MAX;

MemoryMap<dfa_state_t> BinaryDFA::build_quadratic_backward_layer(const DFA& left_in,
                                                                 const DFA& right_in,
                                                                 int layer,
                                                                 const MemoryMap<dfa_state_t>& next_pair_rank_to_output)
{
  size_t curr_layer_count = build_quadratic_read_pairs(layer).size();
  assert(curr_layer_count > 0);

  if(binary_dfa_narrow_pair_count_max > BinaryDFATransitionsHashPlusIndex<false>::pair_rank_max)
    {
      throw std::logic_error("BinaryDFA: binary_dfa_narrow_pair_count_max " + std::to_string(binary_dfa_narrow_pair_count_max) + " exceeds what narrow pair ranks can hold");
    }

  if(curr_layer_count <= binary_dfa_narrow_pair_count_max)
    {
      return build_quadratic_backward_layer<false>(left_in, right_in, layer, curr_layer_count, next_pair_rank_to_output);
    }

  if(curr_layer_count - 1 > BinaryDFATransitionsHashPlusIndex<true>::pair_rank_max)
    {
      throw std::overflow_error("BinaryDFA: layer " + std::to_string(layer) + " has " + std::to_string(curr_layer_count) + " pairs, more than wide pair ranks can number");
    }

  return build_quadratic_backward_layer<true>(left_in, right_in, layer, curr_layer_count, next_pair_rank_to_output);
}

template <bool wide>
MemoryMap<dfa_state_t> BinaryDFA::build_quadratic_backward_layer(const DFA& left_in,
                                                                 const DFA& right_in,
                                                                 int layer,
                                                                 size_t curr_layer_count,
                                                                 const MemoryMap<dfa_state_t>& next_pair_rank_to_output)
{
  typedef BinaryDFATransitionsHashPlusIndex<wide> hash_plus_index_t;
  // Ranks of pairs in this layer, which may exceed dfa_state_t in wide mode.
  typedef std::conditional_t<wide, size_t, dfa_state_t> pair_rank_t;

  Profile profile("build_quadratic_backward_layer");

  profile.set_prefix("layer=" + std::to_string(layer));
  profile.tic(wide ? "init wide" : "init");

  int curr_layer_shape = this->get_layer_shape(layer);
  assert(curr_layer_count - 1 <= hash_plus_index_t::pair_rank_max);

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

  // Up to direct_max transitions, the sort key is the transitions
  // themselves, so the sort leaves states in canonical order (FORMAT-DFA.md
  // section 8) and the scan below numbers them 2, 3, ... in that order.
  // Above it the key is a hash and the order is arbitrary.
  bool direct_key = curr_layer_shape <= hash_plus_index_t::direct_max;
  if(!direct_key)
    {
      hashed_any_layer = true;
    }

  MemoryMap<hash_plus_index_t> curr_transitions_hashed(binary_dir() + "/transitions_hashed", curr_layer_count, [&](size_t i)
  {
    dfa_state_t key[hash_plus_index_t::key_words];
    if(direct_key)
      {
        // copy transitions
        for(int j = 0; j < curr_layer_shape; ++j)
          {
            key[j] = curr_transitions[i * curr_layer_shape + j];
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

        unsigned char hash_output[SHA256_DIGEST_LENGTH];
        static const EVP_MD *hash_implementation = EVP_sha256();
        EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
        bool hash_ok = ((hash_context != NULL) &&
                        (EVP_DigestInit_ex(hash_context, hash_implementation, NULL) == 1) &&
                        (EVP_DigestUpdate(hash_context, &(curr_transitions[i * curr_layer_shape]), curr_layer_shape * sizeof(dfa_state_t)) == 1) &&
                        (EVP_DigestFinal_ex(hash_context, hash_output, 0) == 1));
        EVP_MD_CTX_destroy(hash_context);
        if(!hash_ok)
          {
            throw std::runtime_error("BinaryDFA: SHA-256 of layer " + std::to_string(layer) + " transitions failed");
          }

        static_assert(hash_plus_index_t::key_words * sizeof(dfa_state_t) <= SHA256_DIGEST_LENGTH);
        for(int j = 0; j < hash_plus_index_t::key_words; ++j)
          {
            key[j] = reinterpret_cast<dfa_state_t *>(hash_output)[j];
          }
      }

    hash_plus_index_t output;
    output.set(key, i);

    return output;
  });

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
                    sizeof(dfa_state_t) * curr_layer_shape) != 0;
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
  auto check_new = [&](const hash_plus_index_t& curr_pair_hashed)
  {
    size_t curr_pair_rank = curr_pair_hashed.get_pair_rank();
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

  // Count the new states first, in size_t, so a layer that minimizes to more
  // states than dfa_state_t can number is rejected before the scan below
  // numbers them in dfa_state_t. Ids run 2 .. new_states + 1.
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
  if(layer_size_expected > DFA_STATE_MAX)
    {
      throw std::overflow_error("BinaryDFA: layer " + std::to_string(layer) + " minimizes to " + std::to_string(layer_size_expected) + " states from " + std::to_string(curr_layer_count) + " pairs, more than DFA_STATE_MAX");
    }

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
  assert(layer_size == layer_size_expected);

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
    size_t curr_pair_rank = curr_pairs_permutation[curr_pairs_permutation_index];
    std::copy_n(curr_transitions_begin + size_t(curr_pair_rank) * curr_layer_shape, curr_layer_shape, transitions_out);
  };

  build_layer(layer, layer_size, populate_transitions);

  profile.tic("output");

  // Write pair_rank_to_output by scattering through the permutation:
  // position i of the sorted order holds pair rank curr_pairs_permutation[i]
  // and output id curr_pairs_permutation_to_output[i]. Reads are sequential
  // and every write lands in a distinct slot, since the permutation is a
  // bijection on pair ranks. Slots start at DFA_STATE_MAX, which no output
  // id or constant can equal (ids are below layer_size <= DFA_STATE_MAX), so
  // a slot still holding it afterwards means the permutation missed a rank.

  MemoryMap<dfa_state_t> curr_pair_rank_to_output(binary_build_file_prefix(layer) + "-pair_rank_to_output", curr_layer_count);
  TRY_PARALLEL_3(std::fill,
                 curr_pair_rank_to_output.begin(),
                 curr_pair_rank_to_output.end(),
                 dfa_state_t(DFA_STATE_MAX));

  const pair_rank_t *curr_pairs_permutation_data = curr_pairs_permutation.begin();
  dfa_state_t *curr_pair_rank_to_output_data = curr_pair_rank_to_output.begin();
  TRY_PARALLEL_3(std::for_each,
                 curr_pairs_permutation.begin(),
                 curr_pairs_permutation.end(),
                 [&](const pair_rank_t& curr_pair_rank_ref)
                 {
                   size_t curr_pairs_permutation_index = size_t(&curr_pair_rank_ref - curr_pairs_permutation_data);
                   size_t curr_pair_rank = curr_pair_rank_ref;
                   assert(curr_pair_rank < curr_layer_count);
                   if(check_constant(curr_pair_rank))
                     {
                       curr_pair_rank_to_output_data[curr_pair_rank] = curr_transitions[curr_pair_rank * curr_layer_shape];
                     }
                   else
                     {
                       curr_pair_rank_to_output_data[curr_pair_rank] = curr_pairs_permutation_to_output[curr_pairs_permutation_index];
                     }
                 });

  profile.tic("output check");

  auto unwritten = TRY_PARALLEL_3(std::find,
                                  curr_pair_rank_to_output.begin(),
                                  curr_pair_rank_to_output.end(),
                                  dfa_state_t(DFA_STATE_MAX));
  if(unwritten != curr_pair_rank_to_output.end())
    {
      throw std::logic_error("BinaryDFA: layer " + std::to_string(layer) + " pair rank " + std::to_string(unwritten - curr_pair_rank_to_output.begin()) + " missing from pairs_permutation");
    }

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

  // Filtering and dropping adjacent duplicates both remove very few pairs
  // here (0.04% and 0.008% at cram_7x7 layer 40), so compacting for them
  // would rewrite nearly the whole file to save almost nothing. Instead
  // overwrite those pairs in place with a sentinel that sorts after every
  // real pair, which dirties only the pages holding them. The sort moves the
  // sentinels to the end, where they are cut off before the one compaction,
  // the unique after the sort, which removes about half the pairs.

  profile.tic("mark");

  auto filter_func = get_filter_func();

  // No real pair can equal this: next states are below the next layer's
  // size, which is at most DFA_STATE_MAX.
  const dfa_state_pair_t sentinel(DFA_STATE_MAX, DFA_STATE_MAX);

  auto working_begin = curr_transition_pairs.begin();
  auto working_end = curr_transition_pairs.end();
  size_t working_count = size_t(working_end - working_begin);

  // Each block is scanned serially, comparing every pair with the original
  // value of the one before it. A block's first pair is not compared, since
  // the pair before it belongs to another block that may be overwriting it.
  // That misses at most one duplicate per block, which the unique after the
  // sort removes anyway.
  size_t block_size = std::max(working_count / (8 * std::max(size_t(std::thread::hardware_concurrency()), size_t(1))), size_t(1) << 16);
  size_t block_count = (working_count + block_size - 1) / block_size;
  std::vector<size_t> blocks(block_count);
  std::iota(blocks.begin(), blocks.end(), size_t(0));

  std::atomic<bool> found_sentinel(false);
  std::vector<size_t> block_marked(block_count, 0);
  auto mark_block = [&](size_t block)
  {
    size_t i_begin = block * block_size;
    size_t i_end = std::min(i_begin + block_size, working_count);
    bool has_previous = false;
    dfa_state_pair_t previous;
    size_t marked = 0;
    for(size_t i = i_begin; i < i_end; ++i)
      {
        dfa_state_pair_t current = working_begin[i];
        if(current == sentinel)
          {
            found_sentinel = true;
          }

        bool filtered = filter_func(current.get_left_state(), current.get_right_state());
        bool duplicate = has_previous && (current == previous);
        if(filtered || duplicate)
          {
            working_begin[i] = sentinel;
            ++marked;
          }

        previous = current;
        has_previous = true;
      }
    block_marked[block] = marked;
  };
  TRY_PARALLEL_3(std::for_each, blocks.begin(), blocks.end(), mark_block);

  if(found_sentinel)
    {
      throw std::logic_error("BinaryDFA: layer " + std::to_string(layer + 1) + " transition pairs include the sentinel pair");
    }

  size_t marked_count = std::accumulate(block_marked.begin(), block_marked.end(), size_t(0));
  std::cout << "pair count = " << (working_count - marked_count) << " (filtered and pre sort unique, marked)" << std::endl;

  profile.tic("sort");

  parallel_sort(working_begin, working_end);

  profile.tic("post-unique");

  working_end = std::lower_bound(working_begin, working_end, sentinel);
  if(size_t(working_end - working_begin) != working_count - marked_count)
    {
      throw std::logic_error("BinaryDFA: layer " + std::to_string(layer + 1) + " has " + std::to_string(working_end - working_begin) + " pairs before the sentinels after sorting, expected " + std::to_string(working_count - marked_count));
    }

  working_end = std::unique(working_begin, working_end);

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
