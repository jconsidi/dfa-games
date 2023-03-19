// DFAUtil.cpp

#include "DFAUtil.h"

#include <iostream>
#include <limits>
#include <map>
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
  return double(dfa_a->states()) * double(dfa_b->states());
}

shared_dfa_ptr _reduce_associative_commutative(std::function<shared_dfa_ptr(shared_dfa_ptr, shared_dfa_ptr)> reduce_func,
					       const std::vector<shared_dfa_ptr>& dfas_in)
{
  if(dfas_in.size() <= 0)
    {
      throw std::logic_error("dfas_in is empty");
    }

  std::vector<shared_dfa_ptr> dfas_todo(dfas_in);

  // TODO : make the total number of score evaluations quadratic
  // instead of cubic.
  while(dfas_todo.size() >= 2)
    {
      // find pair with cheapest bound on combination

      int best_i = 0;
      int best_j = 1;
      double best_score = std::numeric_limits<double>::infinity();

      for(int i = 0; i < dfas_todo.size() - 1; ++i)
	{
	  for(int j = i + 1; j < dfas_todo.size(); ++j)
	    {
	      double score = _binary_score(dfas_todo[i], dfas_todo[j]);
	      if(score < best_score)
		{
		  best_i = i;
		  best_j = j;
		  best_score = score;
		}
	    }
	}

      // merge this pair and drop the last entry

      shared_dfa_ptr& dfa_i = dfas_todo[best_i];
      shared_dfa_ptr& dfa_j = dfas_todo[best_j];

      if((dfa_i->states() >= 1024) || (dfa_j->states() >= 1024))
	{
	  std::cout << "  merging DFAs with " << dfa_i->states() << " states and " << dfa_j->states() << " states (" << (dfas_todo.size() - 2) << " remaining)" << std::endl;
	}

      dfa_i = reduce_func(dfa_i, dfa_j);
      dfa_j = dfas_todo.back();
      dfas_todo.pop_back();
    }
  assert(dfas_todo.size() == 1);

  shared_dfa_ptr output = dfas_todo[0];
  if(output->states() >= 1024)
    {
      std::cout << "  merged DFA has " << output->states() << " states and " << output->size() << " positions" << std::endl;
    }
  return output;
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

  return shared_dfa_ptr(new ChangeDFA(*dfa_in, changes_in));
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

  return _singleton_if_constant(shared_dfa_ptr(new IntersectionDFA(*left_in, *right_in)));
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

  return _singleton_if_constant(shared_dfa_ptr(new UnionDFA(*left_in, *right_in)));
}

shared_dfa_ptr DFAUtil::get_union_vector(const dfa_shape_t& shape_in, const std::vector<shared_dfa_ptr>& dfas_in)
{
  if(dfas_in.size() <= 0)
    {
      return get_reject(shape_in);
    }

  return _reduce_associative_commutative(get_union, dfas_in);
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
