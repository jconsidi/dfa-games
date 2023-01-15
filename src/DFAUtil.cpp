// DFAUtil.cpp

#include "DFAUtil.h"

#include <iostream>
#include <map>
#include <queue>
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

shared_dfa_ptr _reduce_associative_commutative(std::function<shared_dfa_ptr(shared_dfa_ptr, shared_dfa_ptr)> reduce_func,
					       const std::vector<shared_dfa_ptr>& dfas_in)
{
  if(dfas_in.size() <= 0)
    {
      throw std::logic_error("dfas_in is empty");
    }

  typedef struct reverse_less
  {
    bool operator()(std::shared_ptr<const DFA> a,
		    std::shared_ptr<const DFA> b) const
    {
      return a->states() > b->states();
    }
  } reverse_less;

  std::priority_queue<shared_dfa_ptr, std::vector<shared_dfa_ptr>, reverse_less> dfa_queue;

  for(int i = 0; i < dfas_in.size(); ++i)
    {
      dfa_queue.push(dfas_in[i]);
    }

  while(dfa_queue.size() > 1)
    {
      std::shared_ptr<const DFA> last = dfa_queue.top();
      dfa_queue.pop();
      std::shared_ptr<const DFA> second_last = dfa_queue.top();
      dfa_queue.pop();

      if(second_last->states() >= 1024)
	{
	  std::cout << "  merging DFAs with " << last->states() << " states and " << second_last->states() << " states (" << dfa_queue.size() << " remaining)" << std::endl;
	}
      dfa_queue.push(reduce_func(second_last, last));
    }
  assert(dfa_queue.size() == 1);

  shared_dfa_ptr output = dfa_queue.top();
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
