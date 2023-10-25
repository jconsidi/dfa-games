// DFAUtil.cpp

#include "DFAUtil.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>

#include "AcceptDFA.h"
#include "ChangeDFA.h"
#include "CountCharacterDFA.h"
#include "DifferenceDFA.h"
#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "Profile.h"
#include "RejectDFA.h"
#include "StringDFA.h"
#include "UnionDFA.h"

double _binary_score(shared_dfa_ptr dfa_a, shared_dfa_ptr dfa_b)
{
  int ndim = dfa_a->get_shape_size();

  const DFALinearBound& bound_a = dfa_a->get_linear_bound();
  const DFALinearBound& bound_b = dfa_b->get_linear_bound();
  bool might_intersect = true;

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

shared_dfa_ptr _reduce_associative_commutative(std::function<shared_dfa_ptr(shared_dfa_ptr, shared_dfa_ptr)> reduce_func,
					       const std::vector<shared_dfa_ptr>& dfas_in)
{
  Profile profile("_reduce_associative_commutative");

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

  for(int i = 0; i < dfas_in.size(); ++i)
    {
      shared_dfa_ptr dfa_i = dfas_in[i];
      dfas_todo.insert(dfa_i);

      for(int j = i + 1; j < dfas_in.size(); ++j)
	{
	  shared_dfa_ptr dfa_j = dfas_in[j];
	  enqueue_pair(dfa_i, dfa_j);
	}
    }

  // unmap DFAs to reduce open files

  for(shared_dfa_ptr dfa : dfas_todo)
    {
      dfa->munmap();
    }

  // work through the priority queue

  while(dfas_todo.size() > 1)
    {
      std::tuple<double, std::string, std::string, shared_dfa_ptr, shared_dfa_ptr> scored_pair = scored_pairs.top();
      scored_pairs.pop();

      // check if both DFAs are still around because we lazy cleaning up scored pairs.

      shared_dfa_ptr dfa_i = std::get<3>(scored_pair);
      shared_dfa_ptr dfa_j = std::get<4>(scored_pair);
      if(!dfas_todo.contains(dfa_i) || !dfas_todo.contains(dfa_j))
	{
	  continue;
	}

      // remove both DFAs from remaining set

      dfas_todo.erase(dfa_i);
      dfas_todo.erase(dfa_j);

      // combine these DFAs and add to remaining set and score pairs

      if((dfa_i->states() >= 1024) || (dfa_j->states() >= 1024))
	{
	  std::cout << "  merging DFAs with " << dfa_i->states() << " states and " << dfa_j->states() << " states (" << dfas_todo.size() << " remaining)" << std::endl;
	}

      shared_dfa_ptr dfa_reduced = reduce_func(dfa_i, dfa_j);
      for(shared_dfa_ptr dfa_k : dfas_todo)
	{
	  enqueue_pair(dfa_reduced, dfa_k);
	}

      dfas_todo.insert(dfa_reduced);

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
	  std::cout << "  merged DFA has " << output->states() << " states and " << output->size() << " positions" << std::endl;
	}

      return output;
    }

  assert(0);
}

std::string _shape_string(const dfa_shape_t& shape_in)
{
  std::ostringstream oss;
  oss << shape_in[0];

  int ndim = shape_in.size();
  for(int layer = 1; layer < ndim; ++layer)
    {
      oss << "/" << shape_in[layer];
    }

  return oss.str();
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

shared_dfa_ptr _try_load(const dfa_shape_t& shape_in, std::string name_in)
{
  try
    {
      return shared_dfa_ptr(new DFA(shape_in, name_in));
    }
  catch(const std::runtime_error& e)
    {
      return shared_dfa_ptr();
    }
}

shared_dfa_ptr DFAUtil::dedupe_by_hash(shared_dfa_ptr dfa_in)
{
  std::string hash = dfa_in->get_hash();

  shared_dfa_ptr previous = load_by_hash(dfa_in->get_shape(), hash);
  if(previous)
    {
      // TODO : sanity check that this is the same DFA
      return previous;
    }

  dfa_in->save_by_hash();
  return dfa_in;
}

shared_dfa_ptr DFAUtil::from_string(const DFAString& string_in)
{
  std::vector<DFAString> strings = {string_in};
  return shared_dfa_ptr(new StringDFA(strings));
}

shared_dfa_ptr DFAUtil::from_strings(const dfa_shape_t& shape_in, const std::vector<DFAString>& strings_in)
{
  // degenerate case

  if(strings_in.size() <= 0)
    {
      return get_reject(shape_in);
    }

  // build new DFA

  return shared_dfa_ptr(new StringDFA(strings_in));
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

  shared_dfa_ptr output(new AcceptDFA(shape_in));
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

  dfa_in = dedupe_by_hash(dfa_in);

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

  shared_dfa_ptr change_previous = _try_load(dfa_in->get_shape(), change_name);
  if(change_previous)
    {
      return change_previous;
    }

  shared_dfa_ptr change_new = dedupe_by_hash(shared_dfa_ptr(new ChangeDFA(*dfa_in, changes_in)));
  change_new->save(change_name);
  return change_new;
}

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_in)
{
  shared_dfa_ptr output(new CountCharacterDFA(shape_in, c_in, count_in));
  output->set_name("get_count_character(" + std::to_string(c_in) + ", " + std::to_string(count_in) + ")");
  return output;
}

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max)
{
  shared_dfa_ptr output(new CountCharacterDFA(shape_in, c_in, count_min, count_max));
  output->set_name("get_count_character(" + std::to_string(c_in) + ", " + std::to_string(count_min) + ", " + std::to_string(count_max) + ")");
  return output;
}

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min)
{
  shared_dfa_ptr output(new CountCharacterDFA(shape_in, c_in, count_min, count_max, layer_min));
  output->set_name("get_count_character(" + std::to_string(c_in) + ", " + std::to_string(count_min) + ", " + std::to_string(count_max) + ", " + std::to_string(layer_min) + ")");
  return output;
}

shared_dfa_ptr DFAUtil::get_count_character(const dfa_shape_t& shape_in, int c_in, int count_min, int count_max, int layer_min, int layer_max)
{
  shared_dfa_ptr output(new CountCharacterDFA(shape_in, c_in, count_min, count_max, layer_min, layer_max));
  output->set_name("get_count_character(" + std::to_string(c_in) + ", " + std::to_string(count_min) + ", " + std::to_string(count_max) + ", " + std::to_string(layer_min) + ", " + std::to_string(layer_max) + ")");
  return output;
}

shared_dfa_ptr DFAUtil::get_difference(shared_dfa_ptr left_in, shared_dfa_ptr right_in)
{
  Profile profile("get_difference");

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
}

shared_dfa_ptr DFAUtil::get_fixed(const dfa_shape_t& shape_in, int fixed_layer, int fixed_character)
{
  static std::map<std::string, shared_dfa_ptr> singletons;

  std::string singleton_key = _shape_string(shape_in) + (" " + std::to_string(fixed_layer) + "/" + std::to_string(fixed_character));
  auto search = singletons.find(singleton_key);
  if(search != singletons.end())
    {
      return search->second;
    }

  shared_dfa_ptr output = shared_dfa_ptr(new FixedDFA(shape_in, fixed_layer, fixed_character));
  output->set_name("get_fixed(fixed_layer=" + std::to_string(fixed_layer) + ", fixed_character=" + std::to_string(fixed_character) + ")");
  singletons[singleton_key] = output;
  return output;
}

shared_dfa_ptr DFAUtil::get_intersection(shared_dfa_ptr left_in, shared_dfa_ptr right_in)
{
  Profile profile("get_intersection");

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

  left_in = dedupe_by_hash(left_in);
  right_in = dedupe_by_hash(right_in);

  if(left_in->get_hash() == right_in->get_hash())
    {
      return left_in;
    }

  if(left_in->get_hash() > right_in->get_hash())
    {
      std::swap(left_in, right_in);
    }

  std::string intersection_name = "intersection_cache/" + left_in->get_hash() + "_" + right_in->get_hash();
  shared_dfa_ptr intersection_previous = _try_load(left_in->get_shape(), intersection_name);
  if(intersection_previous)
    {
      return intersection_previous;
    }

  shared_dfa_ptr intersection_new = dedupe_by_hash(shared_dfa_ptr(new IntersectionDFA(*left_in, *right_in)));
  intersection_new->save(intersection_name);
  return intersection_new;
}

shared_dfa_ptr DFAUtil::get_intersection_vector(const dfa_shape_t& shape_in, const std::vector<shared_dfa_ptr>& dfas_in)
{
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

  return _reduce_associative_commutative(get_intersection, nonlinear_staging);
}

shared_dfa_ptr DFAUtil::get_inverse(shared_dfa_ptr dfa_in)
{
  Profile profile("get_inverse");

  // TODO : short-circuit logic for constants?
  return shared_dfa_ptr(new InverseDFA(*dfa_in));
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

  shared_dfa_ptr output(new RejectDFA(shape_in));
  singletons[singleton_key] = output;
  return output;
}

shared_dfa_ptr DFAUtil::get_union(shared_dfa_ptr left_in, shared_dfa_ptr right_in)
{
  Profile profile("get_union");

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

  left_in = dedupe_by_hash(left_in);
  right_in = dedupe_by_hash(right_in);

  if(left_in->get_hash() == right_in->get_hash())
    {
      return left_in;
    }

  if(left_in->get_hash() > right_in->get_hash())
    {
      std::swap(left_in, right_in);
    }

  std::string union_name = "union_cache/" + left_in->get_hash() + "_" + right_in->get_hash();
  shared_dfa_ptr union_previous = _try_load(left_in->get_shape(), union_name);
  if(union_previous)
    {
      return union_previous;
    }

  if((left_in->states() >= 1024) || (right_in->states() >= 1024))
    {
      std::cout << "UNION " << left_in->get_hash() << " " << right_in->get_hash() << std::endl;
    }

  shared_dfa_ptr union_new = dedupe_by_hash(shared_dfa_ptr(new UnionDFA(*left_in, *right_in)));
  union_new->save(union_name);
  return union_new;
}

shared_dfa_ptr DFAUtil::get_union_vector(const dfa_shape_t& shape_in, const std::vector<shared_dfa_ptr>& dfas_in)
{
  if(dfas_in.size() <= 0)
    {
      return get_reject(shape_in);
    }

  return _reduce_associative_commutative(get_union, dfas_in);
}

shared_dfa_ptr DFAUtil::load_by_hash(const dfa_shape_t& shape_in, std::string hash_in)
{
#if 1
  std::string name = "dfas_by_hash/" + hash_in;
  return _try_load(shape_in, name);
#else
  // test mode with hash cache disabled
  return shared_dfa_ptr(0);
#endif
}

std::string DFAUtil::quick_stats(shared_dfa_ptr dfa_in)
{
  std::ostringstream stats_builder;

  size_t states = dfa_in->states();
  stats_builder << states << " states";

  if(states <= 1000000)
    {
      stats_builder << ", " << dfa_in->size() << " positions";
    }

  return stats_builder.str();
}
