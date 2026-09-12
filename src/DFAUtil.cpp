// DFAUtil.cpp

#include "DFAUtil.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

#include "AcceptDFA.h"
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

double _binary_score(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b)
{
  int ndim = dfa_a->get_shape_size();

#ifdef BINARY_SCORE_LINEAR_BOUND
  const DFALinearBound& bound_a = dfa_a->get_linear_bound();
  const DFALinearBound& bound_b = dfa_b->get_linear_bound();
  bool might_intersect = true;
#endif

  std::vector<double> states_max = {1};
  for(int layer = 1; layer < ndim; ++layer)
    {
      int layer_shape = dfa_a->get_layer_shape(layer);

      std::vector<double> layer_bounds;

      // fanout bound
      layer_bounds.push_back(states_max.back() * layer_shape);

      // product bound
      layer_bounds.push_back(double(dfa_a->get_layer_size(layer)) *
			     double(dfa_b->get_layer_size(layer)));

#ifdef BINARY_SCORE_LINEAR_BOUND
      // inference from linear bounds showing potential intersection
      if(might_intersect)
	{
	  bool layer_shares_characters = false;
	  for(int c = 0; c < layer_shape; ++c)
	    {
	      if(bound_a.check_bound(layer, c) &&
		 bound_b.check_bound(layer, c))
		{
		  layer_shares_characters = true;
		  break;
		}
	    }
	  if(!layer_shares_characters)
	    {
	      might_intersect = false;
	    }
	}
      if(!might_intersect)
	{
	  layer_bounds.push_back(double(dfa_a->get_layer_size(layer)) +
				 double(dfa_b->get_layer_size(layer)));
	}
#endif

      // done for this layer

      double layer_best = *(std::min_element(layer_bounds.begin(), layer_bounds.end()));
      states_max.push_back(layer_best);
    }
  assert(states_max.size() == ndim);

  double total_max = 0.0;
  for(int i = 0; i < ndim; ++i)
    {
      total_max += states_max[i];
    }

  return total_max;
}

shared_dfa_ptr _reduce_aci(std::function<shared_dfa_ptr(shared_dfa_ptr, shared_dfa_ptr)> reduce_func,
			   const std::vector<shared_dfa_ptr>& dfas_in)
{
  // reduce DFAs assuming reduce function is associative, commutative, and idempotent.

  Profile profile("_reduce_aci");

  if(dfas_in.size() <= 0)
    {
      throw std::logic_error("dfas_in is empty");
    }

  if(dfas_in.size() == 1)
    {
      return dfas_in[0];
    }

  // build priority queue to optimize reduce costs

  std::set<shared_dfa_ptr> dfas_todo;
  std::priority_queue<std::tuple<double, std::string, std::string, shared_dfa_ptr, shared_dfa_ptr>> scored_pairs;

  auto enqueue_pair = [&](shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b)
  {
    if(dfa_a->get_hash() > dfa_b->get_hash())
      {
	std::swap(dfa_a, dfa_b);
      }

    scored_pairs.emplace(-_binary_score(dfa_a, dfa_b), dfa_a->get_hash(), dfa_b->get_hash(), dfa_a, dfa_b);
  };

  // add distinct DFAs and queue up pairs

  for(shared_dfa_ptr dfa_i : dfas_in)
    {
      if(dfas_todo.contains(dfa_i))
	{
	  continue;
	}

      for(shared_dfa_ptr dfa_j : dfas_todo)
	{
	  enqueue_pair(dfa_i, dfa_j);
	}

      dfas_todo.insert(dfa_i);
    }

  // unmap DFAs to reduce open files

  for(shared_dfa_ptr dfa : dfas_todo)
    {
      dfa->munmap();
    }

  // work through the priority queue

  while(dfas_todo.size() > 1)
    {
      size_t start_size = dfas_todo.size();

      std::tuple<double, std::string, std::string, shared_dfa_ptr, shared_dfa_ptr> scored_pair = scored_pairs.top();
      scored_pairs.pop();

      // check if both DFAs are still around because we lazy cleaning up scored pairs.

      shared_dfa_ptr dfa_i = std::get<3>(scored_pair);
      shared_dfa_ptr dfa_j = std::get<4>(scored_pair);
      assert(dfa_i != dfa_j);
      if(!dfas_todo.contains(dfa_i) || !dfas_todo.contains(dfa_j))
	{
	  continue;
	}

      // remove both DFAs from remaining set

      dfas_todo.erase(dfa_i);
      dfas_todo.erase(dfa_j);

      assert(dfas_todo.size() == start_size - 2);

      // combine these DFAs and add to remaining set and score pairs

      if((dfa_i->states() >= 1024) || (dfa_j->states() >= 1024))
	{
	  std::cout << "  merging DFAs with " << dfa_i->states() << " states and " << dfa_j->states() << " states (" << dfas_todo.size() << " remaining)" << std::endl;
	}

      shared_dfa_ptr dfa_reduced = reduce_func(dfa_i, dfa_j);
      if(!dfas_todo.contains(dfa_reduced))
	{
	  for(shared_dfa_ptr dfa_k : dfas_todo)
	    {
	      enqueue_pair(dfa_reduced, dfa_k);
	    }

	  dfas_todo.insert(dfa_reduced);
	  assert(dfas_todo.size() == start_size - 1);
	}

      // unmap the DFAs just accessed
      dfa_i->munmap();
      dfa_j->munmap();
      dfa_reduced->munmap();
    }

  // done

  assert(dfas_todo.size() == 1);

  for(shared_dfa_ptr output : dfas_todo)
    {
      if(output->states() >= 1024)
	{
	  std::cout << "  merged DFA has " << DFAUtil::quick_stats(output) << std::endl;
	}

      return output;
    }

  assert(0);
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

  return _reduce_aci(get_intersection, nonlinear_staging);
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

  return _reduce_aci(get_union, dfas_in);
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
