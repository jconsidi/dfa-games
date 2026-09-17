// DFAUtil.cpp

#include "DFAUtil.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sstream>
#include <unordered_map>

#include "AcceptDFA.h"
#include "BinaryDFA.h"
#include "ChangeDFA.h"
#include "CountCharacterDFA.h"
#include "DFA.h"
#include "DifferenceDFA.h"
#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "Profile.h"
#include "RejectDFA.h"
#include "StringDFA.h"
#include "UnionDFA.h"
#include "parallel.h"

// Content-addressed cache key for a whole (sorted, deduped) vector of
// operands: SHA-256 over the concatenation of their own hashes. A single
// operand hash is 64 hex characters, so joining tens of them into one path
// component the way get_intersection/get_union join a pair would risk
// running into filesystem filename length limits; hashing the join keeps
// the key fixed size regardless of how many operands _reduce_nary sees.
std::string _hash_join(const std::vector<shared_dfa_ptr>& dfas_in)
{
  unsigned char digest[SHA256_DIGEST_LENGTH];
  static const EVP_MD *hash_implementation = EVP_sha256();
  EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
  EVP_DigestInit_ex(hash_context, hash_implementation, NULL);

  for(const shared_dfa_ptr& dfa : dfas_in)
    {
      std::string hash = dfa->get_hash();
      EVP_DigestUpdate(hash_context, hash.data(), hash.size());
    }

  EVP_DigestFinal_ex(hash_context, digest, 0);
  EVP_MD_CTX_destroy(hash_context);

  std::ostringstream oss;
  for(size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
      oss << std::hex << std::setw(2) << std::setfill('0') << int(digest[i]);
    }

  return oss.str();
}

// A single BinaryDFA n-ary build keeps one tuples file per layer on disk
// for its entire forward pass (see BinaryDFA.cpp's build_nary_forward), and
// each row of every one of those files is one dfa_state_t per operand --
// so peak disk footprint grows with operand count, on top of however many
// layers the shape has. A single call over the 50+ operands this function
// targets can outrun local scratch before the forward pass even finishes:
// solving cram_7x7's move graph did exactly that, 880GB in by layer 30 of
// 49. Cap how many operands reach one BinaryDFA build directly; beyond
// that, reduce in batches and then reduce the batch results -- a shallow
// tree instead of one wide call. Still far fewer BinaryDFA builds than the
// fully pairwise fold this constructor replaced, and union/intersection
// are associative and idempotent, so the tree shape cannot change the
// result -- only how much operand-file width is ever live in one build.
//
// There is no single right value, so this is overridable via
// DFA_REDUCE_NARY_WIDTH_MAX rather than fixed: too high and a single
// build's own forward pass can still exhaust scratch, which is the
// failure this cap exists to avoid in the first place; too low and the
// *batch results* -- each one a real, cached DFA under
// union_vector_cache/intersection_vector_cache, not scratch that gets
// cleaned up when one build finishes -- accumulate across more tree nodes
// before enough of them exist to fold together. It is a tradeoff between
// two ways of running out of the same disk, shaped by how many layers the
// game's shape has and how much scratch is actually available, neither of
// which this code can know on its own. Read fresh on every call (like
// ScratchConfig's env vars) rather than cached once, so a test -- or a
// long-running process -- can change it without a restart.
static const char *env_reduce_nary_width_max = "DFA_REDUCE_NARY_WIDTH_MAX";

static size_t get_reduce_nary_width_max()
{
  const size_t default_width_max = 16;

  const char *env_value = std::getenv(env_reduce_nary_width_max);
  if((env_value == 0) || (env_value[0] == '\0'))
    {
      return default_width_max;
    }

  errno = 0;
  char *end = 0;
  long parsed = std::strtol(env_value, &end, 10);
  if(errno || (*end != '\0') || (parsed < 2))
    {
      // < 2 is rejected, not just <= 0: a cap of 1 would still see
      // dfas_in.size() > width_max for any real call, batch into
      // single-operand groups that each return unchanged (the size == 1
      // early return above), and recurse on an unshrunk vector forever.
      throw std::runtime_error(std::string(env_reduce_nary_width_max) +
                                " must be an integer >= 2, got \"" + env_value + "\"");
    }

  return size_t(parsed);
}

// Backend for get_intersection_vector/get_union_vector: BinaryDFA's n-ary
// constructor, cached under a key built from the whole (sorted, deduped)
// operand set rather than relying on reuse of pairwise sub-results the way
// _reduce_aci's fold through get_intersection/get_union did. dfas_in is
// taken by value since it is sorted and deduped in place -- union and
// intersection are idempotent, and BinaryDFA's vector constructor does not
// dedupe on its own (see its declaration in BinaryDFA.h).
shared_dfa_ptr _reduce_nary(const dfa_shape_t& shape_in, bool is_union_in, std::vector<shared_dfa_ptr> dfas_in)
{
  Profile profile("_reduce_nary");

  assert(dfas_in.size() >= 1);

  std::sort(dfas_in.begin(), dfas_in.end(), [](const shared_dfa_ptr& a, const shared_dfa_ptr& b)
  {
    return a->get_hash() < b->get_hash();
  });
  dfas_in.erase(std::unique(dfas_in.begin(), dfas_in.end(), [](const shared_dfa_ptr& a, const shared_dfa_ptr& b)
  {
    return a->get_hash() == b->get_hash();
  }), dfas_in.end());

  if(dfas_in.size() == 1)
    {
      return dfas_in[0];
    }

  size_t width_max = get_reduce_nary_width_max();
  if(dfas_in.size() > width_max)
    {
      size_t n = dfas_in.size();

      // Top-down, not bottom-up: always split into width_max batches (n is
      // already known to exceed width_max here), sized as evenly as
      // possible, and recurse into each -- rather than choosing the fewest
      // batches that individually fit within width_max and only then
      // evening those out. The two agree once n fits in two levels, but
      // bottom-up's batch count depends on n / width_max, so as n grows the
      // resulting batches keep landing at very different widths from one
      // call to the next (a change in n by one operand can change how many
      // batches result, and by how much they need to be evened, in a way
      // that has nothing to do with disk safety). Fixing the branching
      // factor at width_max makes every internal node of the recursion
      // split its input the same way regardless of n, and each recursive
      // call re-applies the same rule to its own share -- so a batch that
      // is itself still over width_max keeps dividing by width_max again
      // until it is not, rather than landing wherever n / num_batches
      // happened to fall. batch_results ends up exactly width_max entries,
      // at or under the cap, so combining them needs no further splitting.
      size_t num_batches = width_max;
      // n just over width_max would otherwise split into one non-trivial
      // batch plus width_max - 1 singleton pass-throughs (n = width_max +
      // 1 is the extreme case). Shrink num_batches while one fewer would
      // still, after this same rule reapplies to each of its batches one
      // level down, cover n: (num_batches - 1) batches of up to
      // (num_batches - 1) each cover (num_batches - 1)^2 operands in two
      // levels, so if that already reaches n there is no need for the
      // extra top-level batch.
      while((num_batches - 1) * (num_batches - 1) >= n)
        {
          --num_batches;
        }
      size_t base_batch_size = n / num_batches;
      size_t remainder = n % num_batches;

      std::vector<shared_dfa_ptr> batch_results;
      size_t batch_start = 0;
      for(size_t batch_index = 0; batch_index < num_batches; ++batch_index)
        {
          size_t batch_size = base_batch_size + ((batch_index < remainder) ? 1 : 0);

          std::vector<shared_dfa_ptr> batch;
          for(size_t i = batch_start; i < batch_start + batch_size; ++i)
            {
              batch.push_back(dfas_in[i]);
            }
          batch_start += batch_size;

          batch_results.push_back(_reduce_nary(shape_in, is_union_in, std::move(batch)));
        }
      assert(batch_start == n);

      return _reduce_nary(shape_in, is_union_in, std::move(batch_results));
    }

  std::string cache_name = (is_union_in ? "union_vector_cache/" : "intersection_vector_cache/") + _hash_join(dfas_in);
  return DFAUtil::load_or_build(shape_in, cache_name, [&]()
  {
    return shared_dfa_ptr(new BinaryDFA(dfas_in, is_union_in));
  });
}

std::string _shape_string(const dfa_shape_t& shape_in)
{
  std::ostringstream oss;
  oss << shape_in[0];

  int ndim = int(shape_in.size());
  for(int layer = 1; layer < ndim; ++layer)
    {
      oss << "/" << shape_in[layer];
    }

  return oss.str();
}

// _shape_string is also used as an in-memory map key with "/" between
// layers, which is fine there but unusable as a single path component --
// it would nest one subdirectory per layer. This is the same shape
// flattened for use in a cache directory name below.
std::string _shape_path(const dfa_shape_t& shape_in)
{
  std::string s = _shape_string(shape_in);
  std::replace(s.begin(), s.end(), '/', '_');
  return s;
}

shared_dfa_ptr _singleton_if_constant(shared_dfa_ptr dfa_in)
{
  dfa_state_t initial_state = dfa_in->get_initial_state();
  if(initial_state == 0)
    {
      return DFAUtil::get_reject(dfa_in->get_shape());
    }
  else if(initial_state == 1)
    {
      return DFAUtil::get_accept(dfa_in->get_shape());
    }

  return dfa_in;
}

shared_dfa_ptr _try_load(const dfa_shape_t& shape_in, std::string name_in, bool durable)
{
  try
    {
      return shared_dfa_ptr(new DFA(shape_in, name_in, durable));
    }
  catch(const std::runtime_error& e)
    {
      return shared_dfa_ptr();
    }
}

uint64_t DFAUtil::for_each_position(shared_dfa_ptr dfa_in, std::function<void(const DFAString&)> func)
{
  // Enumeration stays on this thread. DFAIterator is sequential, and the
  // lazy mmap behind DFA::get_transitions is not thread safe, so workers are
  // handed DFAString values and never touch the DFA. Positions are collected
  // a batch at a time and each batch is then checked in parallel.

  constexpr size_t batch_size = 4096;

  dfa_in->mmap();

  std::vector<DFAString> batch;
  batch.reserve(batch_size);

  std::mutex failure_mutex;
  std::optional<size_t> failure_index;
  std::exception_ptr failure;
  std::atomic<bool> failed(false);

  uint64_t count = 0;

  auto iter = dfa_in->cbegin();
  auto end = dfa_in->cend();
  while(iter < end)
    {
      batch.clear();
      for(; (iter < end) && (batch.size() < batch_size); ++iter)
	{
	  batch.push_back(*iter);
	}

      const DFAString *batch_first = batch.data();
      TRY_PARALLEL_PAR_3(std::for_each, batch.begin(), batch.end(), [&](const DFAString& position)
      {
	if(failed.load(std::memory_order_relaxed))
	  {
	    // another position in this batch already failed
	    return;
	  }

	try
	  {
	    func(position);
	  }
	catch(...)
	  {
	    size_t index = size_t(&position - batch_first);

	    std::lock_guard<std::mutex> guard(failure_mutex);
	    if(!failure_index || (index < *failure_index))
	      {
		// keep the earliest failure so the reported position does
		// not depend on how the batch was scheduled
		failure_index = index;
		failure = std::current_exception();
	      }
	    failed.store(true, std::memory_order_relaxed);
	  }
      });

      if(failed.load())
	{
	  // batches run in order, so this is the first failing position
	  std::rethrow_exception(failure);
	}

      count += batch.size();
    }

  return count;
}

shared_dfa_ptr DFAUtil::from_string(const dfa_shape_t& shape_in, const DFAString& string_in)
{
  std::vector<DFAString> strings = {string_in};
  return shared_dfa_ptr(new StringDFA(shape_in, strings));
}

shared_dfa_ptr DFAUtil::from_strings(const dfa_shape_t& shape_in, const std::vector<DFAString>& strings_in)
{
  // degenerate case

  if(strings_in.size() <= 0)
    {
      return get_reject(shape_in);
    }

  // build new DFA

  return shared_dfa_ptr(new StringDFA(shape_in, strings_in));
}

shared_dfa_ptr DFAUtil::get_accept(const dfa_shape_t& shape_in)
{
  // returns a singleton per shape

  static std::map<std::string, shared_dfa_ptr> singletons;

  std::string singleton_key = _shape_string(shape_in);
  auto search = singletons.find(singleton_key);
  if(search != singletons.end())
    {
      return search->second;
    }

  // Saved immediately (cache-style, not durable) rather than built and left
  // temporary: this map holds every entry for the life of the process, and
  // a temporary DFA's build/ staging directory stays open for exactly as
  // long as something holds the DFA -- process lifetime here, which on a
  // long build is real accumulated disk usage, not a theoretical one.
  shared_dfa_ptr output = load_or_build(shape_in,
					 "accept_cache/" + _shape_path(shape_in),
					 [&]()
					 {
					   return shared_dfa_ptr(new AcceptDFA(shape_in));
					 });
  singletons[singleton_key] = output;
  return output;
}

shared_dfa_ptr DFAUtil::get_change(shared_dfa_ptr dfa_in, const change_vector& changes_in)
{
  Profile profile("get_change");

  if(dfa_in->is_constant(0))
    {
      return DFAUtil::get_reject(dfa_in->get_shape());
    }

  // check for NOP change

  bool changes_found = false;
  for(change_optional layer_change : changes_in)
    {
      if(layer_change.has_value())
	{
	  changes_found = true;
	  break;
	}
    }
  if(!changes_found)
    {
      return dfa_in;
    }

  // cached change

  std::ostringstream oss;
  oss << "change_cache/";
  oss << dfa_in->get_hash();
  for(int layer = 0; layer < changes_in.size(); ++layer)
    {
      change_optional layer_change = changes_in[layer];
      if(layer_change.has_value())
	{
	  oss << "_" << layer << "=" << std::get<0>(*layer_change) << "," << std::get<1>(*layer_change);
	}
    }
  std::string change_name = oss.str();
  return load_or_build(dfa_in->get_shape(),
		       change_name,
		       [&]()
		       {
			 return shared_dfa_ptr(new ChangeDFA(*dfa_in, changes_in));
		       });
}

// Not memoized in memory the way get_accept/get_reject/get_fixed are --
// callers do not reuse the same (shape, args) enough to be worth an
// in-memory map -- but still routed through load_or_build rather than
// built and returned temporary: at least one caller (ChessGame's en
// passant clearing condition) stores the result directly in a MoveGraph's
// node conditions, which lives for the life of the game object. Left
// temporary, that DFA's build/ staging directory would stay open for as
// long as the game object does.

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_in)
{
  std::string name_in = "count_character_cache/" + _shape_path(shape_in) + "_c" + std::to_string(c_in) + "_n" + std::to_string(count_in);
  return load_or_build(shape_in, name_in, [&]()
  {
    return shared_dfa_ptr(new CountCharacterDFA(shape_in, c_in, count_in));
  });
}

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max)
{
  std::string name_in = "count_character_cache/" + _shape_path(shape_in) + "_c" + std::to_string(c_in) + "_" + std::to_string(count_min) + "_" + std::to_string(count_max);
  return load_or_build(shape_in, name_in, [&]()
  {
    return shared_dfa_ptr(new CountCharacterDFA(shape_in, c_in, count_min, count_max));
  });
}

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min)
{
  std::string name_in = "count_character_cache/" + _shape_path(shape_in) + "_c" + std::to_string(c_in) + "_" + std::to_string(count_min) + "_" + std::to_string(count_max) + "_" + std::to_string(layer_min);
  return load_or_build(shape_in, name_in, [&]()
  {
    return shared_dfa_ptr(new CountCharacterDFA(shape_in, c_in, count_min, count_max, layer_min));
  });
}

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min, int layer_max)
{
  std::string name_in = "count_character_cache/" + _shape_path(shape_in) + "_c" + std::to_string(c_in) + "_" + std::to_string(count_min) + "_" + std::to_string(count_max) + "_" + std::to_string(layer_min) + "_" + std::to_string(layer_max);
  return load_or_build(shape_in, name_in, [&]()
  {
    return shared_dfa_ptr(new CountCharacterDFA(shape_in, c_in, count_min, count_max, layer_min, layer_max));
  });
}

shared_dfa_ptr DFAUtil::get_difference(shared_dfa_ptr left_in, shared_dfa_ptr right_in)
{
  Profile profile("get_difference");

  if((left_in == right_in) || (left_in->get_hash() == right_in->get_hash()))
    {
      return get_reject(left_in->get_shape());
    }

  std::string difference_name = "difference_cache/" + left_in->get_hash() + "_" + right_in->get_hash();

  return load_or_build(left_in->get_shape(), difference_name, [&]()
  {
    if(left_in->is_constant(true))
      {
	return get_inverse(right_in);
      }
    else if(left_in->is_constant(false))
      {
	return get_reject(left_in->get_shape());
      }

    if(right_in->is_constant(true))
      {
	return get_reject(left_in->get_shape());
      }
    else if(right_in->is_constant(false))
      {
	return left_in;
      }

    return _singleton_if_constant(shared_dfa_ptr(new DifferenceDFA(*left_in, *right_in)));
  });
}

shared_dfa_ptr DFAUtil::get_fixed(const dfa_shape_t& shape_in, int fixed_layer, int fixed_character)
{
  // returns a singleton per (shape, fixed_layer, fixed_character): this is
  // MoveGraph's per-edge condition builder (MoveGraph.cpp), called once for
  // every move in every game's move graph, so the in-memory map matters here
  // for more than just avoiding a rebuild.

  static std::map<std::string, shared_dfa_ptr> singletons;

  std::string singleton_key = _shape_string(shape_in) + (" " + std::to_string(fixed_layer) + "/" + std::to_string(fixed_character));
  auto search = singletons.find(singleton_key);
  if(search != singletons.end())
    {
      return search->second;
    }

  // Saved immediately rather than left temporary -- see get_accept above
  // for why: this map, like that one, holds every entry for the life of
  // the process.
  std::string name_in = "fixed_cache/" + _shape_path(shape_in) + "_" + std::to_string(fixed_layer) + "_" + std::to_string(fixed_character);
  shared_dfa_ptr output = load_or_build(shape_in,
					 name_in,
					 [&]()
					 {
					   return shared_dfa_ptr(new FixedDFA(shape_in, fixed_layer, fixed_character));
					 });
  singletons[singleton_key] = output;
  return output;
}

shared_dfa_ptr DFAUtil::get_intersection(shared_dfa_ptr left_in, shared_dfa_ptr right_in)
{
  Profile profile("get_intersection");

  assert(left_in);
  assert(right_in);

  if(left_in->is_constant(true))
    {
      return right_in;
    }
  else if(left_in->is_constant(false))
    {
      return get_reject(left_in->get_shape());
    }

  if(right_in->is_constant(true))
    {
      return left_in;
    }
  else if(right_in->is_constant(false))
    {
      return get_reject(left_in->get_shape());
    }

  if(left_in->get_hash() == right_in->get_hash())
    {
      return left_in;
    }

  if(left_in->get_hash() > right_in->get_hash())
    {
      std::swap(left_in, right_in);
    }

  std::string intersection_name = "intersection_cache/" + left_in->get_hash() + "_" + right_in->get_hash();
  return load_or_build(left_in->get_shape(), intersection_name, [&]()
  {
    bool left_linear = left_in->is_linear();
    bool right_linear = right_in->is_linear();

    if(left_linear || right_linear)
      {
	// use linear bounds to check if either DFA is a subset of the
	// other DFA. in that case, we can immediately return the
	// subset.
	//
	// This is only sound because get_linear_bound() is documented (see
	// DFA.h) to return a *tight* bound -- exactly the characters some
	// accepted string uses at each layer, not merely a sound
	// over-approximation -- for any DFA without dead states, which
	// is_linear()==true implies here. A <= comparison against a loose
	// bound would only be sound reasoning about the bounds themselves,
	// not about the languages one of these branches returns instead of
	// actually computing.

	const DFALinearBound& left_bound = left_in->get_linear_bound();
	const DFALinearBound& right_bound = right_in->get_linear_bound();

	if(left_linear && right_linear)
	  {
	    // both linear
	    if(left_bound <= right_bound)
	      {
		return left_in;
	      }
	    else if(right_bound <= left_bound)
	      {
		return right_in;
	      }
	  }
	else if(left_linear)
	  {
	    // only left linear
	    if(right_bound <= left_bound)
	      {
		return right_in;
	      }
	  }
	else
	  {
	    // only right linear
	    if(left_bound <= right_bound)
	      {
		return left_in;
	      }
	  }
      }

    if((left_in->states() >= 1024) || (right_in->states() >= 1024))
      {
	std::cout << "INTERSECTION " << left_in->get_hash() << " " << right_in->get_hash() << std::endl;
      }

    return shared_dfa_ptr(new IntersectionDFA(*left_in, *right_in));
  });
}

shared_dfa_ptr DFAUtil::get_intersection_vector(const dfa_shape_t& shape_in, const std::vector<shared_dfa_ptr>& dfas_in)
{
  for(shared_dfa_ptr dfa: dfas_in)
    {
      assert(dfa.get());
    }

  shared_dfa_ptr linear_staging = get_accept(shape_in);
  std::vector<shared_dfa_ptr> nonlinear_staging;

  // separate linear DFAs from non-linear DFAs.
  for(const shared_dfa_ptr& dfa: dfas_in)
    {
      if(dfa->is_linear())
	{
	  // combine all the linear DFAs since that is cheap.
	  linear_staging = get_intersection(linear_staging, dfa);
	}
      else
	{
	  nonlinear_staging.push_back(dfa);
	}
    }

  if(nonlinear_staging.size() == 0)
    {
      // all input DFAs were linear
      return linear_staging;
    }

  // quickly shrink all the non-linear DFAs by intersecting with the
  // combined linear DFA.
  for(int i = 0; i < nonlinear_staging.size(); ++i)
    {
      nonlinear_staging[i] = get_intersection(nonlinear_staging[i], linear_staging);
    }

  return _reduce_nary(shape_in, false, nonlinear_staging);
}

shared_dfa_ptr DFAUtil::get_inverse(shared_dfa_ptr dfa_in)
{
  Profile profile("get_inverse");

  std::string inverse_name = "inverse_cache/" + dfa_in->get_hash();

  return load_or_build(dfa_in->get_shape(), inverse_name, [&]()
  {
    return shared_dfa_ptr(new InverseDFA(*dfa_in));
  });
}

shared_dfa_ptr DFAUtil::get_reject(const dfa_shape_t& shape_in)
{
  // returns a singleton per shape

  static std::map<std::string, shared_dfa_ptr> singletons;

  std::string singleton_key = _shape_string(shape_in);
  auto search = singletons.find(singleton_key);
  if(search != singletons.end())
    {
      return search->second;
    }

  // Saved immediately -- see get_accept above for why.
  shared_dfa_ptr output = load_or_build(shape_in,
					 "reject_cache/" + _shape_path(shape_in),
					 [&]()
					 {
					   return shared_dfa_ptr(new RejectDFA(shape_in));
					 });
  singletons[singleton_key] = output;
  return output;
}

shared_dfa_ptr DFAUtil::get_union(shared_dfa_ptr left_in, shared_dfa_ptr right_in)
{
  Profile profile("get_union");

  assert(left_in);
  assert(right_in);

  if(left_in->is_constant(true))
    {
      return get_accept(left_in->get_shape());
    }
  else if(left_in->is_constant(false))
    {
      return right_in;
    }

  if(right_in->is_constant(true))
    {
      return get_accept(right_in->get_shape());
    }
  else if(right_in->is_constant(false))
    {
      return left_in;
    }

  if(left_in->get_hash() == right_in->get_hash())
    {
      return left_in;
    }

  if(left_in->get_hash() > right_in->get_hash())
    {
      std::swap(left_in, right_in);
    }

  auto check_output = [&](shared_dfa_ptr dfa_out)
  {
    // paranoid checks
    assert(dfa_out->size() >= left_in->size());
    assert(dfa_out->size() >= right_in->size());
  };

  std::string union_name = "union_cache/" + left_in->get_hash() + "_" + right_in->get_hash();
  shared_dfa_ptr output =
    load_or_build(left_in->get_shape(),
                  union_name,
                  [&]()
                  {
                    if((left_in->states() >= 1024) || (right_in->states() >= 1024))
                      {
                        std::cout << "UNION " << left_in->get_hash() << " " << right_in->get_hash() << std::endl;
                      }

                    return shared_dfa_ptr(new UnionDFA(*left_in, *right_in));
                  });

  // paranoid checks
  check_output(output);

  return output;
}

shared_dfa_ptr DFAUtil::get_union_vector(const dfa_shape_t& shape_in, const std::vector<shared_dfa_ptr>& dfas_in)
{
  for(shared_dfa_ptr dfa: dfas_in)
    {
      assert(dfa.get());
    }

  if(dfas_in.size() <= 0)
    {
      return get_reject(shape_in);
    }

  if(dfas_in.size() >= 50)
    {
      std::cout << "UNION VECTOR";
      for(const shared_dfa_ptr& dfa : dfas_in)
	{
	  std::cout << " " << dfa->get_hash();
	}
      std::cout << std::endl;
    }

  return _reduce_nary(shape_in, true, dfas_in);
}

shared_dfa_ptr DFAUtil::load_by_hash(const dfa_shape_t& shape_in, std::string hash_in, bool durable)
{
#if 1
  // Keyed by root as well as hash: the same hash can legitimately exist
  // under both roots (content addressing gives identical bytes either way),
  // but a cache hit here must not let a durable=false lookup silently
  // succeed off the back of an archive-only copy, or vice versa -- the two
  // roots are meant to be checkable independently of one another.
  static std::unordered_map<std::string, std::weak_ptr<const DFA>> _dfas_by_hash;

  std::string cache_key = (durable ? "durable:" : "cache:") + hash_in;

  auto search = _dfas_by_hash.find(cache_key);
  if(search != _dfas_by_hash.end())
    {
      std::weak_ptr<const DFA> weak_dfa = search->second;
      if(shared_dfa_ptr dfa = weak_dfa.lock())
	{
	  return dfa;
	}
    }

  std::string name = "dfas_by_hash/" + hash_in;
  shared_dfa_ptr dfa = _try_load(shape_in, name, durable);
  if(dfa)
    {
      _dfas_by_hash[cache_key] = dfa;
    }
  return dfa;
#else
  // test mode with hash cache disabled
  return shared_dfa_ptr(0);
#endif
}

shared_dfa_ptr DFAUtil::load_by_name(const dfa_shape_t& shape_in, std::string name_in, bool durable)
{
  Profile profile("load " + name_in);

  std::optional<std::string> hash = DFA::parse_hash(name_in, durable);
  if(hash)
    {
      return load_by_hash(shape_in, *hash, durable);
    }

  return shared_dfa_ptr(new DFA(shape_in, name_in, durable));
}

shared_dfa_ptr DFAUtil::load_or_build(const dfa_shape_t& shape_in, std::string name_in, std::function<shared_dfa_ptr()> build_func, bool durable)
{
  Profile profile("load_or_build " + name_in);

  profile.tic("load");
  try
    {
      shared_dfa_ptr output = load_by_name(shape_in, name_in, durable);
      if(output)
	{
	  output->set_name("saved(\"" + name_in + "\")");
	  std::cout << "loaded " << name_in << " => " << DFAUtil::quick_stats(output) << std::endl;

	  return output;
	}
    }
  catch(const std::runtime_error& e)
    {
    }

  profile.tic("build");
  std::cout << "building " << name_in << std::endl;
  shared_dfa_ptr output = build_func();
  output->set_name("saved(\"" + name_in + "\")");

  profile.tic("stats");
  std::cout << "built " << name_in << " => " << output->states() << " states" << std::endl;

  profile.tic("save");
  if(durable)
    {
      output->save_durable(name_in);
    }
  else
    {
      output->save_cache(name_in);
    }
  return output;
}

std::string DFAUtil::quick_stats(shared_dfa_ptr dfa_in)
{
  std::ostringstream stats_builder;

  size_t states = dfa_in->states();
  stats_builder << states << " states";

  if(states <= 100000)
    {
      stats_builder << ", " << dfa_in->size() << " positions";
    }

  return stats_builder.str();
}
