// MinimizeDFA.cpp

#include "MinimizeDFA.h"

#include <cstring>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "Profile.h"
#include "parallel.h"

static std::string minimize_build_file_name(int layer, std::string suffix)
{
  std::ostringstream filename_builder;
  filename_builder << "scratch/minimizedfa/layer=" << (layer < 10 ? "0" : "") << layer;
  filename_builder << "-" << suffix;
  return filename_builder.str();
}

MinimizeDFA::MinimizeDFA(const DFA& dfa_in)
  : DFA(dfa_in.get_shape())
{
  Profile profile("MinimizeDFA");

  dfa_in.mmap();

    // initially empty since shortcircuiting will happen by last layer.

  MemoryMap<dfa_state_t> next_state_to_output(2); // dummy initialization
  next_state_to_output[0] = 0;
  next_state_to_output[1] = 1;

  for(int layer = get_shape_size() - 1; layer >= 0; --layer)
    {
      assert(next_state_to_output.size() == dfa_in.get_layer_size(layer + 1));
      MemoryMap<dfa_state_t> curr_state_to_output = minimize_layer(dfa_in, layer, next_state_to_output);
      assert(curr_state_to_output.size() == dfa_in.get_layer_size(layer));
      assert(curr_state_to_output[0] == dfa_state_t(0));
      assert(curr_state_to_output[1] == dfa_state_t(1));

      next_state_to_output.unlink();
      std::swap(curr_state_to_output, next_state_to_output);
    }

  this->set_initial_state(next_state_to_output[dfa_in.get_initial_state()]);
}

MemoryMap<dfa_state_t> MinimizeDFA::minimize_layer(const DFA& dfa_in, int layer, const MemoryMap<dfa_state_t>& next_state_to_output)
{
  Profile profile("minimize_layer");

  assert(next_state_to_output.size() == dfa_in.get_layer_size(layer + 1));

  profile.tic("hash transitions");

  int curr_layer_shape = get_layer_shape(layer);
  dfa_state_t curr_layer_size = dfa_in.get_layer_size(layer);

  MemoryMap<MinimizeDFATransitionsHashPlusIndex> transitions_hashed("scratch/minimizedfa/transitions_hashed", curr_layer_size, [&](size_t i)
  {
    DFATransitionsReference state_transitions = dfa_in.get_transitions(layer, dfa_state_t(i));

    MinimizeDFATransitionsHashPlusIndex output;
    if(curr_layer_shape + 1 <= minimize_dfa_hash_width)
      {
        // copy transitions
        for(int j = 0; j < curr_layer_shape; ++j)
          {
            output.data[j] = state_transitions[j];
          }
        // and zero pad the rest of the hash space
        for(int j = curr_layer_shape; j < minimize_dfa_hash_width - 1; ++j)
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
        EVP_DigestUpdate(hash_context, state_transitions.data(), curr_layer_shape * sizeof(dfa_state_t));
        EVP_DigestFinal_ex(hash_context, hash_output, 0);

        for(int j = 0; j < minimize_dfa_hash_width - 1; ++j)
          {
            output.data[j] = reinterpret_cast<dfa_state_t *>(hash_output)[j];
          }
      }

    output.data[minimize_dfa_hash_width - 1] = i;

    return output;
  });

  profile.tic("hash sort");

  TRY_PARALLEL_2(std::sort, transitions_hashed.begin(), transitions_hashed.end());

#if 0
  profile.tic("hash check");

  auto hash_collision = TRY_PARALLEL_3(std::adjacent_find, transitions_hashed.begin(), transitions_hashed.end(), [&](const MinimizeDFATransitionsHashPlusIndex& a, const MinimizeDFATransitionsHashPlusIndex& b)
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
#endif

  // make permutation of pairs sorted by transitions

  profile.tic("sort permutation");

  assert(curr_layer_size <= size_t(UINT32_MAX));
  MemoryMap<dfa_state_t> permutation("scratch/minimizedfa/permutation", curr_layer_size, [&](size_t i)
  {
    return transitions_hashed[i].get_curr_state();
  });

  profile.tic("states identification");

  MemoryMap<dfa_state_t> permutation_to_new_state("scratch/minimizedfa/permutation_to_new_state", size_t(curr_layer_size));

  auto check_constant = [&](dfa_state_t curr_state)
  {
    DFATransitionsReference state_transitions = dfa_in.get_transitions(layer, curr_state);

    dfa_state_t possible_constant = state_transitions[0];
    if(possible_constant >= 2)
      {
        return false;
      }

    for(int j = 1; j < curr_layer_shape; ++j)
      {
        if(state_transitions[j] != possible_constant)
          {
            return false;
          }
      }

    return true;
  };

  auto transitions_hashed_begin = transitions_hashed.begin();
  auto check_new = [&](const MinimizeDFATransitionsHashPlusIndex& curr_pair_hashed)
  {
    dfa_state_t curr_state = curr_pair_hashed.get_curr_state();
    if(check_constant(curr_state))
      {
        return dfa_state_t(0);
      }

    if(&curr_pair_hashed <= transitions_hashed_begin)
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
                 transitions_hashed.begin(),
                 transitions_hashed.end(),
                 permutation_to_new_state.begin(),
                 [](dfa_state_t previous, dfa_state_t delta) {
                   dfa_state_t output = previous + delta;
                   // check for overflow
                   assert(output >= previous);
                   return output;
                 },
                 check_new,
                 1); // first new state will be 2

  dfa_state_t layer_size = permutation_to_new_state[curr_layer_size - 1] + 1;

  profile.tic("hash unlink");

  transitions_hashed.unlink();

  profile.tic("states write");

  auto permutation_to_new_state_begin = permutation_to_new_state.begin();
  auto permutation_to_new_state_end = permutation_to_new_state.end();
  auto populate_transitions = [&](dfa_state_t new_state_id, dfa_state_t *transitions_out)
  {
    assert(new_state_id < layer_size);
    auto iter = std::lower_bound(permutation_to_new_state_begin,
                                 permutation_to_new_state_end,
                                 new_state_id);
    assert(iter < permutation_to_new_state_end);
    assert(*iter == new_state_id);

    dfa_state_t permutation_index = iter - permutation_to_new_state_begin;
    dfa_state_t old_state_id = permutation[permutation_index];

    DFATransitionsReference old_state_transitions = dfa_in.get_transitions(layer, old_state_id);
    for(int j = 0; j < curr_layer_shape; ++j)
      {
        transitions_out[j] = next_state_to_output[old_state_transitions[j]];
      }
  };

  build_layer(layer, layer_size, populate_transitions);

  profile.tic("invert");

  // invert permutation so we can write pair_rank_to_output in order

  MemoryMap<dfa_state_t> permutation_inverse("scratch/minimizedfa/pairs_permutation_inverse", size_t(curr_layer_size));
  // contents are indexes into permutation
  std::iota(permutation_inverse.begin(),
            permutation_inverse.end(),
            0);
  // sort so permutation[permutation_inverse[i]] = i

  TRY_PARALLEL_3(std::sort,
                 permutation_inverse.begin(),
                 permutation_inverse.end(),
                 [&](dfa_state_t a, dfa_state_t b) {
                   return permutation[a] < permutation[b];
                 });

  for(dfa_state_t i : {size_t(0), size_t(curr_layer_size - 1)})
    {
      assert(permutation[permutation_inverse[i]] == i);
    }

  profile.tic("unlink pairs_permutation");

  permutation.unlink();

  profile.tic("output");

  MemoryMap<dfa_state_t> curr_state_to_output(minimize_build_file_name(layer, "state_to_output"), curr_layer_size, [&](dfa_state_t old_state_id)
  {
    DFATransitionsReference old_state_transitions = dfa_in.get_transitions(layer, old_state_id);

    if(check_constant(old_state_id))
      {
        return old_state_transitions[0];
      }

    dfa_state_t permutation_index = permutation_inverse[old_state_id];
    return permutation_to_new_state[permutation_index];
  });

  assert(curr_state_to_output.size() > 0);

  // shrink state

  profile.tic("unlink permutation_inverse");

  permutation_inverse.unlink();

  profile.tic("unlink permutation_to_new_state");

  permutation_to_new_state.unlink();

  // cleanup in destructors
  profile.tic("cleanup");

  // done

  return curr_state_to_output;
}
