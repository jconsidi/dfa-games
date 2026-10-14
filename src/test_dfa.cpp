// test_union_dfa.cpp

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "AcceptDFA.h"
#include "BinaryDFA.h"
#include "ChangeDFA.h"
#include "CountCharacterDFA.h"
#include "CountDFA.h"
#include "DFA.h"
#include "DFAUtil.h"
#include "DifferenceDFA.h"
#include "FixedDFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "RejectDFA.h"
#include "TestDFAParams.h"
#include "StringDFA.h"
#include "sort_utils.h"
#include "UnionDFA.h"

std::string get_parameter_string(const dfa_shape_t& shape)
{
  std::ostringstream builder;
  builder << shape.size() << "=" << shape[0];
  for(int layer = 1; layer < shape.size(); ++layer)
    {
      builder << "/" << shape[layer];
    }

  return builder.str();
}

void test_union_pair(std::string test_name, const DFA& left, const DFA& right, size_t expected_boards);

void test_helper(std::string test_name, const DFA& test_dfa, size_t expected_boards)
{
  double actual_boards = test_dfa.size();
  if(size_t(actual_boards) != expected_boards)
    {
      std::cerr << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ": expected " << expected_boards << std::endl;
      std::cerr << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ":   actual " << actual_boards << std::endl;

      throw std::logic_error(test_name + ": test failed");
    }
  std::cout << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ": passed" << std::endl;
  std::cout.flush();
}

void test_intersection_pair(std::string test_name, const DFA& left, const DFA& right, size_t expected_boards)
{
  std::cout << "checking intersection pair " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA test_dfa(left, right);
  test_helper("intersection pair " + test_name, test_dfa, expected_boards);
}

void test_intersection_pair(std::string test_name, const DFA& left, const DFA& right, double expected_boards)
{
  test_intersection_pair(test_name, left, right, size_t(expected_boards));
}

// Checks DFAUtil::get_intersection's is_linear() shortcut directly, against
// the always-correct pairwise IntersectionDFA constructor. This is the
// actual code path that was wrong before get_linear_bound() was fixed to
// aggregate only states reachable from initial_state: is_linear()==true
// does not by itself mean get_linear_bound() has no unreachable-but-live
// state polluting some layer's bound (DedupedDFA-built DFAs, CountDFA
// among them, do not prune unreachable states), so the shortcut's
// left_bound <= right_bound check could wrongly treat one operand as a
// subset of the other. See DFA.h's comment on get_linear_bound() for the
// invariant this now depends on.
void test_intersection_via_util(std::string test_name, shared_dfa_ptr left, shared_dfa_ptr right, size_t expected_boards)
{
  std::cout << "checking intersection via util " << test_name << std::endl;
  std::cout.flush();

  shared_dfa_ptr test_dfa = DFAUtil::get_intersection(left, right);
  test_helper("intersection via util " + test_name, *test_dfa, expected_boards);

  IntersectionDFA expected(*left, *right);
  if(size_t(test_dfa->size()) != size_t(expected.size()))
    {
      throw std::logic_error("intersection via util " + test_name + ": disagrees with IntersectionDFA");
    }
}

// Checks get_linear_bound()'s own contract (DFA.h) directly, rather than
// only through get_intersection's use of it: for a DFA with no dead states,
// the bound at each layer is exactly the set of characters some accepted
// string uses there, not merely a sound superset of it.
//
// Shape (1, 2, 3, 4) forces this: layer 0 can never hold a piece (shape 1),
// so a position with exactly 3 pieces (the maximum possible, since layers
// 1-3 can hold at most one piece each) must have *every* one of layers 1-3
// non-blank -- there is no choice of which layer is blank, unlike (say)
// "exactly 2 of 3". CountDFA's construction still creates a state at layer
// 1 for "1 piece already, entering layer 1", even though that is
// impossible before any counting layer has been read; before
// get_linear_bound() was restricted to states reachable from
// initial_state, that unreachable state's own (non-uniform) row leaked
// blank into layer 1's bound as if some accepted count-3 position could
// have layer 1 blank, which none can.
void test_linear_bound()
{
  std::cout << "checking linear bound" << std::endl;
  std::cout.flush();

  dfa_shape_t shape({TEST4_DFA_SHAPE});

  shared_dfa_ptr count2(new CountDFA(shape, 2));
  shared_dfa_ptr count3(new CountDFA(shape, 3));

  if(!count3->is_linear())
    {
      throw std::logic_error("linear bound: count3 expected to be linear for this test to be meaningful");
    }

  const DFALinearBound& bound3 = count3->get_linear_bound();

  // layer 0: shape 1, the only character there is always used.
  if(!bound3.check_bound(0, 0))
    {
      throw std::logic_error("linear bound: count3 layer 0 character 0 should be possible");
    }

  // layers 1-3: every count3 position needs blank (character 0) excluded
  // and some non-blank character included, since all three must be
  // non-blank.
  for(int layer = 1; layer <= 3; ++layer)
    {
      if(bound3.check_bound(layer, 0))
	{
	  throw std::logic_error("linear bound: count3 layer " + std::to_string(layer) + " character 0 (blank) should not be possible");
	}

      bool found_nonblank = false;
      for(int c = 1; c < shape[layer]; ++c)
	{
	  found_nonblank = found_nonblank || bound3.check_bound(layer, c);
	}
      if(!found_nonblank)
	{
	  throw std::logic_error("linear bound: count3 layer " + std::to_string(layer) + " should have some non-blank character possible");
	}
    }

  // contrast: count2 allows exactly one of layers 1-3 to be blank, so
  // (unlike count3) blank *is* possible at each of them.
  const DFALinearBound& bound2 = count2->get_linear_bound();
  for(int layer = 1; layer <= 3; ++layer)
    {
      if(!bound2.check_bound(layer, 0))
	{
	  throw std::logic_error("linear bound: count2 layer " + std::to_string(layer) + " character 0 (blank) should be possible");
	}
    }

  std::cout << "linear bound: passed" << std::endl;
  std::cout.flush();
}

void test_inverse(std::string test_name, const DFA& dfa_in)
{
  std::cout << "checking inverse " << test_name << std::endl;
  std::cout.flush();

  size_t accept_all_boards = 1;
  for(auto layer_shape : dfa_in.get_shape())
    {
      accept_all_boards *= layer_shape;
    }
  size_t expected_boards = accept_all_boards - size_t(dfa_in.size());

  InverseDFA test_dfa(dfa_in);
  test_helper("inverse " + test_name, test_dfa, expected_boards);

  IntersectionDFA intersection_dfa(test_dfa, dfa_in);
  test_intersection_pair("inverse " + test_name, test_dfa, dfa_in, size_t(0));

  test_union_pair("inverse union " + test_name, test_dfa, dfa_in, accept_all_boards);
}

void test_union_pair(std::string test_name, const DFA& left, const DFA& right, size_t expected_boards)
{
  std::cout << "checking union pair " << test_name << std::endl;
  std::cout.flush();

  UnionDFA test_dfa(left, right);
  test_helper("union pair " + test_name, test_dfa, expected_boards);
}

void test_union_pair(std::string test_name, const DFA& left, const DFA& right, double expected_boards)
{
  return test_union_pair(test_name, left, right, size_t(expected_boards));
}

std::vector<DFAString> get_all_positions(const dfa_shape_t& shape)
{
  std::vector<std::vector<int>> characters_list(1);
  for(int layer = 0; layer < shape.size(); ++layer)
    {
      std::vector<std::vector<int>> next_characters_list;
      for(const std::vector<int>& prefix : characters_list)
	{
	  for(int c = 0; c < shape[layer]; ++c)
	    {
	      next_characters_list.push_back(prefix);
	      next_characters_list.back().push_back(c);
	    }
	}
      characters_list = next_characters_list;
    }

  std::vector<DFAString> output;
  for(const std::vector<int>& characters : characters_list)
    {
      output.emplace_back(shape, characters);
    }

  return output;
}

void test_states(std::string test_name, const DFA& test_dfa, size_t expected_states)
{
  size_t actual_states = test_dfa.states();
  if(actual_states != expected_states)
    {
      std::cerr << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ": expected " << expected_states << " states" << std::endl;
      std::cerr << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ":   actual " << actual_states << " states" << std::endl;

      throw std::logic_error(test_name + ": test failed");
    }
  std::cout << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ": passed" << std::endl;
  std::cout.flush();
}

// Build the union of left and right with narrow pair ranks and again with
// every backward layer forced into wide ones, and check they agree. Hashed
// layers number states identically in both modes, as do direct ones whose
// transitions fit in wide mode's key, so without a shape 3 layer the DFAs
// must be byte identical. A shape 3 layer is direct in narrow mode and hashed
// in wide mode, so there the numbering may differ and only the language and
// state count are compared.
void test_wide_pair_ranks_pair(std::string test_name, const DFA& left, const DFA& right)
{
  const dfa_shape_t& shape = left.get_shape();
  std::string full_name = get_parameter_string(shape) + " wide pair ranks " + test_name;

  UnionDFA narrow(left, right);

  size_t narrow_pair_count_max_saved = binary_dfa_narrow_pair_count_max;
  binary_dfa_narrow_pair_count_max = 0;
  std::shared_ptr<UnionDFA> wide;
  try
    {
      wide = std::make_shared<UnionDFA>(left, right);
    }
  catch(...)
    {
      binary_dfa_narrow_pair_count_max = narrow_pair_count_max_saved;
      throw;
    }
  binary_dfa_narrow_pair_count_max = narrow_pair_count_max_saved;

  if(wide->states() != narrow.states())
    {
      throw std::logic_error(full_name + ": wide has " + std::to_string(wide->states()) + " states, narrow has " + std::to_string(narrow.states()));
    }

  size_t positions_checked = 0;
  for(const DFAString& position : get_all_positions(shape))
    {
      if(wide->contains(position) != narrow.contains(position))
        {
          throw std::logic_error(full_name + ": wide and narrow disagree on a position");
        }
      ++positions_checked;
    }

  bool has_shape_3 = false;
  for(int layer_shape : shape)
    {
      has_shape_3 = has_shape_3 || (layer_shape == 3);
    }

  if(has_shape_3)
    {
      std::cout << full_name << ": passed (" << narrow.states() << " states, " << positions_checked << " positions; shape 3 layer, bytes not compared)" << std::endl;
    }
  else
    {
      if(wide->get_hash() != narrow.get_hash())
        {
          throw std::logic_error(full_name + ": wide and narrow DFAs are not byte identical");
        }
      std::cout << full_name << ": passed (" << narrow.states() << " states, " << positions_checked << " positions, byte identical)" << std::endl;
    }
  std::cout.flush();
}

// Build the union of left and right with BinaryDFA's sorts on their normal
// path and again forced through parallel_sort_in_place. The in-place sort is
// not stable, but elements that compare equal are identical pairs or hash
// records with identical transitions, so the DFAs must be byte identical.
void test_in_place_sort_pair(std::string test_name, const DFA& left, const DFA& right)
{
  std::string full_name = get_parameter_string(left.get_shape()) + " in-place sort " + test_name;

  UnionDFA normal(left, right);

  size_t divisor_saved = parallel_sort_buffered_memory_divisor;
  parallel_sort_buffered_memory_divisor = SIZE_MAX;
  std::shared_ptr<UnionDFA> in_place;
  try
    {
      in_place = std::make_shared<UnionDFA>(left, right);
    }
  catch(...)
    {
      parallel_sort_buffered_memory_divisor = divisor_saved;
      throw;
    }
  parallel_sort_buffered_memory_divisor = divisor_saved;

  if(in_place->get_hash() != normal.get_hash())
    {
      throw std::logic_error(full_name + ": in-place and normal sorts give different DFAs");
    }
  std::cout << full_name << ": passed (" << normal.states() << " states, byte identical)" << std::endl;
  std::cout.flush();
}

// Build the union of left and right with pair_rank_to_output written in one
// pass and again split into at least slices_min passes. Each pass writes the
// same values into its own ranks, so the DFAs must be byte identical. Small
// layers get more slices than ranks, which runs empty slices too.
void test_output_slices_pair(std::string test_name, const DFA& left, const DFA& right, size_t slices_min)
{
  std::string full_name = get_parameter_string(left.get_shape()) + " output slices>=" + std::to_string(slices_min) + " " + test_name;

  UnionDFA single(left, right);

  size_t slices_min_saved = binary_dfa_output_slices_min;
  binary_dfa_output_slices_min = slices_min;
  std::shared_ptr<UnionDFA> sliced;
  try
    {
      sliced = std::make_shared<UnionDFA>(left, right);
    }
  catch(...)
    {
      binary_dfa_output_slices_min = slices_min_saved;
      throw;
    }
  binary_dfa_output_slices_min = slices_min_saved;

  if(sliced->get_hash() != single.get_hash())
    {
      throw std::logic_error(full_name + ": sliced and single pass outputs give different DFAs");
    }
  std::cout << full_name << ": passed (" << single.states() << " states, byte identical)" << std::endl;
  std::cout.flush();
}

void test_wide_pair_ranks(const dfa_shape_t& shape)
{
  CountDFA count1(shape, 1);
  CountDFA count2(shape, 2);
  CountDFA count3(shape, 3);
  CountCharacterDFA zero1(shape, 0, 1);
  CountCharacterDFA one1(shape, 1, 1);

  test_wide_pair_ranks_pair("count1+count2", count1, count2);
  test_wide_pair_ranks_pair("count2+count3", count2, count3);
  test_wide_pair_ranks_pair("zero1+one1", zero1, one1);

  UnionDFA count12(count1, count2);
  test_wide_pair_ranks_pair("count12+zero1", count12, zero1);

  test_in_place_sort_pair("count1+count2", count1, count2);
  test_in_place_sort_pair("count2+count3", count2, count3);
  test_in_place_sort_pair("zero1+one1", zero1, one1);
  test_in_place_sort_pair("count12+zero1", count12, zero1);

  for(size_t slices_min : {2, 3, 7})
    {
      test_output_slices_pair("count1+count2", count1, count2, slices_min);
      test_output_slices_pair("count2+count3", count2, count3, slices_min);
      test_output_slices_pair("zero1+one1", zero1, one1, slices_min);
      test_output_slices_pair("count12+zero1", count12, zero1, slices_min);
    }
}

// Restores dfa_state_narrow_max when a test that lowered it leaves scope,
// including by an exception.
class NarrowMaxGuard
{
  uint64_t saved;

public:

  explicit NarrowMaxGuard(uint64_t narrow_max)
    : saved(dfa_state_narrow_max)
  {
    dfa_state_narrow_max = narrow_max;
  }

  ~NarrowMaxGuard()
  {
    dfa_state_narrow_max = saved;
  }

  NarrowMaxGuard(const NarrowMaxGuard&) = delete;
  NarrowMaxGuard& operator=(const NarrowMaxGuard&) = delete;
};

// Build each DFA with dfa_state_narrow_max at its production value and again
// lowered, so arrays of state ids go wide at small sizes, and check every
// build is byte identical: storage width must never reach the bytes of a
// DFA. Lowered to 0 every array is wide from the start; lowered to 1 or 3,
// add_state's staging widens partway through a layer.
void test_narrow_max(const dfa_shape_t& shape)
{
  std::string shape_name = get_parameter_string(shape);

  CountDFA count1(shape, 1);
  CountDFA count2(shape, 2);
  CountCharacterDFA zero1(shape, 0, 1);
  FixedDFA fixed(shape, 1, 0);
  IntersectionDFA count2_fixed(count2, fixed);

  std::vector<DFAString> all_positions = get_all_positions(shape);
  std::vector<DFAString> some_positions;
  for(size_t i = 0; i < all_positions.size(); i += 3)
    {
      some_positions.push_back(all_positions[i]);
    }

  change_vector changes(shape.size());
  changes[1] = change_type(0, 1);

  typedef std::function<std::shared_ptr<const DFA>()> build_func;
  std::vector<std::pair<std::string, build_func>> builds = {
    {"count2", [&]() {return std::make_shared<CountDFA>(shape, 2);}},
    {"zero1", [&]() {return std::make_shared<CountCharacterDFA>(shape, 0, 1);}},
    {"strings", [&]() {return std::make_shared<StringDFA>(shape, some_positions);}},
    {"union count1+count2", [&]() {return std::make_shared<UnionDFA>(count1, count2);}},
    {"intersection count2+zero1", [&]() {return std::make_shared<IntersectionDFA>(count2, zero1);}},
    {"difference count2-zero1", [&]() {return std::make_shared<DifferenceDFA>(count2, zero1);}},
    {"inverse count2", [&]() {return std::make_shared<InverseDFA>(count2);}},
    {"change two pass", [&]() {return std::make_shared<ChangeDFA>(count2, changes);}},
    {"change one pass", [&]() {return std::make_shared<ChangeDFA>(count2_fixed, changes);}}};

  for(const auto& [build_name, build] : builds)
    {
      std::string full_name = shape_name + " narrow max " + build_name;

      std::shared_ptr<const DFA> baseline = build();
      for(uint64_t narrow_max : {0, 1, 3})
	{
	  std::shared_ptr<const DFA> lowered;
	  {
	    NarrowMaxGuard guard(narrow_max);
	    lowered = build();
	  }

	  if(lowered->get_hash() != baseline->get_hash())
	    {
	      throw std::logic_error(full_name + ": narrow max " + std::to_string(narrow_max) + " gives a different DFA");
	    }
	}
      std::cout << full_name << ": passed (" << baseline->states() << " states, byte identical at narrow max 0, 1 and 3)" << std::endl;
    }
  std::cout.flush();
}

// Build each binary operation with binary_dfa_key_narrow_max at its
// production value and again at 0, which keys every layer by hashing its
// transitions at 8 bytes each. The states are numbered differently, so the
// bytes differ, but the language and the state count must not. Each lowered
// result is also reloaded from its file, which checks its digest, and is
// built again with dfa_state_narrow_max at 0, which must be byte identical.
void test_key_narrow_max(const dfa_shape_t& shape)
{
  std::string shape_name = get_parameter_string(shape);

  CountDFA count1(shape, 1);
  CountDFA count2(shape, 2);
  CountCharacterDFA zero1(shape, 0, 1);

  typedef std::function<std::shared_ptr<const DFA>()> build_func;
  std::vector<std::pair<std::string, build_func>> builds = {
    {"union count1+count2", [&]() {return std::make_shared<UnionDFA>(count1, count2);}},
    {"intersection count2+zero1", [&]() {return std::make_shared<IntersectionDFA>(count2, zero1);}},
    {"difference count2-zero1", [&]() {return std::make_shared<DifferenceDFA>(count2, zero1);}}};

  std::vector<DFAString> all_positions = get_all_positions(shape);

  for(const auto& [build_name, build] : builds)
    {
      std::string full_name = shape_name + " key narrow max " + build_name;

      std::shared_ptr<const DFA> baseline = build();
      std::shared_ptr<const DFA> lowered;
      std::shared_ptr<const DFA> lowered_wide;
      {
	uint64_t saved = binary_dfa_key_narrow_max;
	binary_dfa_key_narrow_max = 0;
	try
	  {
	    lowered = build();

	    // Wide ids are hashed in place rather than copied out, which must
	    // give the same keys.
	    NarrowMaxGuard guard(0);
	    lowered_wide = build();
	  }
	catch(...)
	  {
	    binary_dfa_key_narrow_max = saved;
	    throw;
	  }
	binary_dfa_key_narrow_max = saved;
      }

      if(lowered_wide->get_hash() != lowered->get_hash())
	{
	  throw std::logic_error(full_name + ": wide ids give different 8 byte keys than narrow ones");
	}

      if(lowered->states() != baseline->states())
	{
	  throw std::logic_error(full_name + ": " + std::to_string(lowered->states()) + " states, default has " + std::to_string(baseline->states()));
	}

      DFA reloaded(shape, "dfas_by_hash/" + lowered->get_hash(), false);
      size_t accepted = 0;
      for(const DFAString& position : all_positions)
	{
	  bool expected = baseline->contains(position);
	  if((lowered->contains(position) != expected) || (reloaded.contains(position) != expected))
	    {
	      throw std::logic_error(full_name + ": language differs from the default build");
	    }
	  accepted += expected;
	}

      std::cout << full_name << ": passed (" << baseline->states() << " states, " << all_positions.size() << " positions with " << accepted << " accepted match, " << (lowered->get_hash() == baseline->get_hash() ? "byte identical" : "renumbered") << ", identical with wide ids)" << std::endl;
    }
  std::cout.flush();
}

void test_string_dfa(const dfa_shape_t& shape)
{
  std::vector<DFAString> all_positions = get_all_positions(shape);

  // a state accepting every suffix is the reserved accept state, so building
  // from every string in the shape must collapse to the accept DFA instead of
  // duplicating state 1 in each layer.

  AcceptDFA accept(shape);

  shared_dfa_ptr string_all = DFAUtil::from_strings(shape, all_positions);
  test_helper("string all", *string_all, all_positions.size());
  test_states("string all states", *string_all, accept.states());

  // dropping one string must stop the collapse, otherwise the check above
  // would also pass for an implementation that always returned accept.

  if(all_positions.size() >= 2)
    {
      std::vector<DFAString> all_but_one(all_positions.begin() + 1, all_positions.end());

      shared_dfa_ptr string_all_but_one = DFAUtil::from_strings(shape, all_but_one);
      test_helper("string all but one", *string_all_but_one, all_but_one.size());
      if(string_all_but_one->states() <= accept.states())
	{
	  throw std::logic_error("string all but one: collapsed to accept");
	}
    }
}

void test_suite(const dfa_shape_t& shape)
{

  // string tests

  test_string_dfa(shape);

  // accept all

  std::shared_ptr<const DFA> accept(new AcceptDFA(shape));
  size_t accept_expected = 1;
  for(auto layer_shape : shape)
    {
      accept_expected *= layer_shape;
    }
  test_helper("accept", *accept, accept_expected);

  // reject all

  std::shared_ptr<const DFA> reject(new RejectDFA(shape));
  test_helper("reject", *reject, 0);

  // count tests

  std::shared_ptr<const DFA> count0(new CountDFA(shape, 0));
  test_helper("count0", *count0, 1);

  std::shared_ptr<const DFA> count1(new CountDFA(shape, 1));
  size_t count1_expected = 0;
  for(auto layer_shape : shape)
    {
      count1_expected += layer_shape - 1;
    }
  test_helper("count1", *count1, count1_expected);

  std::shared_ptr<const DFA> count2(new CountDFA(shape, 2));
  size_t count2_expected = 0;
  for(int l1 = 0; l1 < shape.size(); ++l1)
    {
      for(int l2 = l1 + 1; l2 < shape.size(); ++l2)
	{
	  count2_expected += (shape[l1] - 1) * (shape[l2] - 1);
	}
    }
  test_helper("count2", *count2, count2_expected);

  std::shared_ptr<const DFA> count3(new CountDFA(shape, 3));
  size_t count3_expected = 0;
  for(int l1 = 0; l1 < shape.size(); ++l1)
    {
      for(int l2 = l1 + 1; l2 < shape.size(); ++l2)
	{
	  for(int l3 = l2 + 1; l3 < shape.size(); ++l3)
	    {
	      count3_expected += (shape[l1] - 1) * (shape[l2] - 1) * (shape[l3] - 1);
	    }
	}
    }
  test_helper("count3", *count3, count3_expected);

  // save tests

  std::shared_ptr<DFA> save(new CountDFA(shape, 0));
  save->save_durable("test");
  save = 0; // trigger destructor

  std::shared_ptr<const DFA> load(new DFA(shape, "test", true));
  test_helper("load", *load, 1);

  std::shared_ptr<DFA> save2(new AcceptDFA(shape));
  // overwrite save
  save2->save_durable("test");

  // intersection tests

  test_intersection_pair("count0+count0", *count0, *count0, count0->size());
  test_intersection_pair("count0+count1", *count0, *count1, size_t(0));
  test_intersection_pair("count1+count1", *count1, *count1, count1->size());
  test_intersection_pair("count1+count2", *count1, *count2, size_t(0));
  test_intersection_pair("count2+count1", *count2, *count1, size_t(0));
  test_intersection_pair("count2+count2", *count2, *count2, count2->size());
  test_intersection_pair("count2+count3", *count2, *count3, size_t(0));
  test_intersection_pair("count3+count2", *count3, *count2, size_t(0));
  test_intersection_pair("count3+count3", *count3, *count3, count3->size());

  // DFAUtil::get_intersection's is_linear() shortcut specifically -- see
  // test_intersection_via_util's comment. count2/count3 is the regression
  // case (count3 is_linear() but, before the get_linear_bound() fix, had an
  // unreachable state inflating its bound whenever an earlier layer's
  // shape restricted which counts could actually be reached yet).

  test_intersection_via_util("count0+count0", count0, count0, size_t(count0->size()));
  test_intersection_via_util("count1+count2", count1, count2, size_t(0));
  test_intersection_via_util("count2+count2", count2, count2, size_t(count2->size()));
  test_intersection_via_util("count2+count3", count2, count3, size_t(0));
  test_intersection_via_util("count3+count2", count3, count2, size_t(0));
  test_intersection_via_util("count3+count3", count3, count3, size_t(count3->size()));

  // union tests

  test_union_pair("count0+count0", *count0, *count0, count0->size());
  test_union_pair("count0+count1", *count0, *count1, count0->size() + count1->size());
  test_union_pair("count1+count0", *count1, *count0, count0->size() + count1->size());
  test_union_pair("count1+count1", *count1, *count1, count1->size());
  test_union_pair("count2+count3", *count2, *count3, count2->size() + count3->size());

  // inverse tests

  test_inverse("accept", *accept);
  test_inverse("reject", *reject);
  test_inverse("count0", *count0);
  test_inverse("count1", *count1);
  test_inverse("count2", *count2);
  test_inverse("count3", *count3);

  // count character tests

  std::shared_ptr<const DFA> zero0(new CountCharacterDFA(shape, 0, 0));
  size_t zero0_expected = 1;
  for(int layer = 0; layer < shape.size(); ++layer)
    {
      zero0_expected *= shape[layer] - 1;
    }
  test_helper("zero0", *zero0, zero0_expected);

  std::shared_ptr<const DFA> zero1(new CountCharacterDFA(shape, 0, 1));
  size_t zero1_expected = 0;
  for(int l0 = 0; l0 < shape.size(); ++l0)
    {
      size_t l0_expected = 1;
      for(int lo = 0; lo < shape.size(); ++lo)
	{
	  if(lo != l0)
	    {
	      l0_expected *= shape[lo] - 1;
	    }
	}
      zero1_expected += l0_expected;
    }
  test_helper("zero1", *zero1, zero1_expected);
  test_intersection_pair("zero1 + count1", *zero0, *count1, size_t(0));

  std::shared_ptr<const DFA> one1(new CountCharacterDFA(shape, 1, 1));
  size_t one1_expected = 0;
  for(int l1 = 0; l1 < shape.size(); ++l1)
    {
      if(shape[l1] < 2)
	{
	  // can't be one at this layer
	  continue;
	}

      size_t l1_expected = 1;
      for(int lo = 0; lo < shape.size(); ++lo)
	{
	  if(lo != l1)
	    {
	      l1_expected *= (shape[lo] >= 2) ? (shape[lo] - 1) : 1;
	    }
	}
      one1_expected += l1_expected;
    }
  test_helper("one1", *one1, one1_expected);

  size_t one1_count1_expected = 0;
  for(int layer = 0; layer < shape.size(); ++layer)
    {
      if(shape[layer] >= 2)
	{
	  ++one1_count1_expected;
	}
    }
  test_intersection_pair("one1 + count1", *one1, *count1, one1_count1_expected);
}

// Asks build_layer for one state more than dfa_state_t can name. The
// check fires before anything is allocated, so this is cheap.
class OversizedLayerDFA : public DFA
{
public:
  OversizedLayerDFA()
    : DFA(dfa_shape_t({2}))
  {
    build_layer(0, size_t(DFA_STATE_MAX) + 1, [](dfa_state_t, dfa_state_t *)
    {
      throw std::logic_error("oversized layer: populate_func should not run");
    });
  }
};

void test_oversized_layer()
{
  try
    {
      OversizedLayerDFA dfa;
    }
  catch(const std::overflow_error& e)
    {
      std::cout << "oversized layer: rejected: " << e.what() << std::endl;
      return;
    }

  throw std::logic_error("oversized layer: build_layer accepted DFA_STATE_MAX + 1 states");
}

int main()
{
  try
    {
      test_suite(dfa_shape_t({TEST1_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST2_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST3_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST4_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST5_DFA_SHAPE}));

      test_wide_pair_ranks(dfa_shape_t({TEST2_DFA_SHAPE}));
      test_wide_pair_ranks(dfa_shape_t({TEST3_DFA_SHAPE}));
      test_wide_pair_ranks(dfa_shape_t({TEST4_DFA_SHAPE}));
      test_wide_pair_ranks(dfa_shape_t({TEST5_DFA_SHAPE}));
      test_wide_pair_ranks(dfa_shape_t({2, 4, 5, 2, 4, 5, 2}));

      test_narrow_max(dfa_shape_t({TEST3_DFA_SHAPE}));
      test_narrow_max(dfa_shape_t({TEST5_DFA_SHAPE}));
      test_narrow_max(dfa_shape_t({2, 4, 5, 2, 4, 5, 2}));

      test_key_narrow_max(dfa_shape_t({TEST3_DFA_SHAPE}));
      test_key_narrow_max(dfa_shape_t({TEST5_DFA_SHAPE}));
      test_key_narrow_max(dfa_shape_t({2, 4, 5, 2, 4, 5, 2}));

      test_linear_bound();

      // Must run last: a build abandoned by an exception deliberately
      // leaves build_in_progress set (see DFA.cpp), so any later DFA
      // construction in this process would assert.
      test_oversized_layer();
    }
  catch(const std::logic_error& e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
