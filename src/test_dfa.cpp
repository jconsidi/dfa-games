// test_union_dfa.cpp

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "AcceptDFA.h"
#include "BinaryDFA.h"
#include "CountCharacterDFA.h"
#include "CountDFA.h"
#include "DFA.h"
#include "DFAUtil.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "RejectDFA.h"
#include "StringDFA.h"
#include "TestDFAParams.h"
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

// Checks BinaryDFA's n-ary constructor (the "vector" constructor) two ways:
// against an independently known position count, the same as the pairwise
// checks above, and against the *language* (not just the digest -- the
// pairwise reduction being compared against need not itself be canonical,
// e.g. once a layer is wide enough that get_intersection/get_union fall
// back to a hashed sort key) computed by folding the raw, two-input
// IntersectionDFA/UnionDFA constructor over the same inputs one at a time.
//
// Deliberately not DFAUtil::get_intersection/get_union: those take a
// shortcut when one operand is_linear(), trusting get_linear_bound() to be
// tight for it (see the comment on get_linear_bound() in DFA.h, and
// test_intersection_via_util below, which checks that shortcut directly).
// Folding through DFAUtil here would make this cross check depend on that
// same code path instead of independently verifying against it.
void test_intersection_vector(std::string test_name, const std::vector<shared_dfa_ptr>& dfas_in, size_t expected_boards)
{
  std::cout << "checking intersection vector " << test_name << std::endl;
  std::cout.flush();

  shared_dfa_ptr test_dfa(new BinaryDFA(dfas_in, false));
  test_helper("intersection vector " + test_name, *test_dfa, expected_boards);

  if(!test_dfa->is_canonical())
    {
      throw std::logic_error("intersection vector " + test_name + ": not canonical");
    }

  shared_dfa_ptr expected = dfas_in.at(0);
  for(size_t i = 1; i < dfas_in.size(); ++i)
    {
      expected = shared_dfa_ptr(new IntersectionDFA(*expected, *dfas_in[i]));
    }

  shared_dfa_ptr diff_forward = DFAUtil::get_difference(test_dfa, expected);
  shared_dfa_ptr diff_backward = DFAUtil::get_difference(expected, test_dfa);
  if(!diff_forward->is_constant(false) || !diff_backward->is_constant(false))
    {
      throw std::logic_error("intersection vector " + test_name + ": language mismatch vs pairwise reduction");
    }
}

void test_union_vector(std::string test_name, const std::vector<shared_dfa_ptr>& dfas_in, size_t expected_boards)
{
  std::cout << "checking union vector " << test_name << std::endl;
  std::cout.flush();

  shared_dfa_ptr test_dfa(new BinaryDFA(dfas_in, true));
  test_helper("union vector " + test_name, *test_dfa, expected_boards);

  if(!test_dfa->is_canonical())
    {
      throw std::logic_error("union vector " + test_name + ": not canonical");
    }

  shared_dfa_ptr expected = dfas_in.at(0);
  for(size_t i = 1; i < dfas_in.size(); ++i)
    {
      expected = shared_dfa_ptr(new UnionDFA(*expected, *dfas_in[i]));
    }

  shared_dfa_ptr diff_forward = DFAUtil::get_difference(test_dfa, expected);
  shared_dfa_ptr diff_backward = DFAUtil::get_difference(expected, test_dfa);
  if(!diff_forward->is_constant(false) || !diff_backward->is_constant(false))
    {
      throw std::logic_error("union vector " + test_name + ": language mismatch vs pairwise reduction");
    }
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

// _reduce_nary (DFAUtil.cpp) folds operand counts above
// reduce_nary_width_max through repeated smaller BinaryDFA builds instead
// of one wide one, to bound peak disk usage -- see its own comment. Force
// that path here (more operands than the cap) and check the DFAUtil-level
// result against the same pairwise fold used elsewhere in this file for
// the flat, single-build case, so batched reduction is confirmed to
// produce the exact same language as flat reduction, not just a
// plausible-looking one.
void test_reduce_nary_chunking(const dfa_shape_t& shape)
{
  std::cout << "checking reduce_nary chunking" << std::endl;
  std::cout.flush();

  std::vector<DFAString> positions = get_all_positions(shape);

  const size_t width = 40; // more than one _reduce_nary batch (16 operands)
  assert(width <= positions.size());

  shared_dfa_ptr accept(new AcceptDFA(shape));

  std::vector<shared_dfa_ptr> singletons;
  std::vector<shared_dfa_ptr> accept_minus_one;
  for(size_t i = 0; i < width; ++i)
    {
      shared_dfa_ptr singleton(new StringDFA(shape, std::vector<DFAString>({positions[i]})));
      singletons.push_back(singleton);
      accept_minus_one.push_back(DFAUtil::get_difference(accept, singleton));
    }

  shared_dfa_ptr union_actual = DFAUtil::get_union_vector(shape, singletons);
  shared_dfa_ptr union_expected = singletons[0];
  for(size_t i = 1; i < singletons.size(); ++i)
    {
      union_expected = shared_dfa_ptr(new UnionDFA(*union_expected, *singletons[i]));
    }
  if(!DFAUtil::get_difference(union_actual, union_expected)->is_constant(false) ||
     !DFAUtil::get_difference(union_expected, union_actual)->is_constant(false))
    {
      throw std::logic_error("reduce_nary chunking: union language mismatch vs pairwise reduction");
    }
  test_helper("reduce_nary chunking union", *union_actual, width);

  shared_dfa_ptr intersection_actual = DFAUtil::get_intersection_vector(shape, accept_minus_one);
  shared_dfa_ptr intersection_expected = accept_minus_one[0];
  for(size_t i = 1; i < accept_minus_one.size(); ++i)
    {
      intersection_expected = shared_dfa_ptr(new IntersectionDFA(*intersection_expected, *accept_minus_one[i]));
    }
  if(!DFAUtil::get_difference(intersection_actual, intersection_expected)->is_constant(false) ||
     !DFAUtil::get_difference(intersection_expected, intersection_actual)->is_constant(false))
    {
      throw std::logic_error("reduce_nary chunking: intersection language mismatch vs pairwise reduction");
    }
  test_helper("reduce_nary chunking intersection", *intersection_actual, positions.size() - width);
}

// DFA_REDUCE_NARY_WIDTH_MAX overrides the batch width _reduce_nary uses --
// see its comment in DFAUtil.cpp for why this needs to be tunable rather
// than fixed (too high and a single build's own forward pass can still
// exhaust scratch; too low and cached batch results pile up instead).
// Check that a small override actually forces chunking well below the
// default of 16, on an operand count that would not have chunked at all
// otherwise, and that a nonsense override is rejected loudly rather than
// silently falling back to the default.
void test_reduce_nary_width_max_override(const dfa_shape_t& shape)
{
  std::cout << "checking DFA_REDUCE_NARY_WIDTH_MAX" << std::endl;
  std::cout.flush();

  std::vector<DFAString> positions = get_all_positions(shape);

  const size_t width = 10; // under the default cap (16), over the override below (4)
  assert(width <= positions.size());

  std::vector<shared_dfa_ptr> singletons;
  for(size_t i = 0; i < width; ++i)
    {
      singletons.push_back(shared_dfa_ptr(new StringDFA(shape, std::vector<DFAString>({positions[i]}))));
    }

  setenv("DFA_REDUCE_NARY_WIDTH_MAX", "4", 1);

  shared_dfa_ptr union_actual = DFAUtil::get_union_vector(shape, singletons);

  setenv("DFA_REDUCE_NARY_WIDTH_MAX", "not a number", 1);
  bool threw = false;
  try
    {
      DFAUtil::get_union_vector(shape, singletons);
    }
  catch(const std::runtime_error&)
    {
      threw = true;
    }

  unsetenv("DFA_REDUCE_NARY_WIDTH_MAX");

  shared_dfa_ptr union_expected = singletons[0];
  for(size_t i = 1; i < singletons.size(); ++i)
    {
      union_expected = shared_dfa_ptr(new UnionDFA(*union_expected, *singletons[i]));
    }
  if(!DFAUtil::get_difference(union_actual, union_expected)->is_constant(false) ||
     !DFAUtil::get_difference(union_expected, union_actual)->is_constant(false))
    {
      throw std::logic_error("DFA_REDUCE_NARY_WIDTH_MAX=4: union language mismatch vs pairwise reduction");
    }
  test_helper("DFA_REDUCE_NARY_WIDTH_MAX=4 union", *union_actual, width);

  if(!threw)
    {
      throw std::logic_error("DFA_REDUCE_NARY_WIDTH_MAX=\"not a number\": expected an error, got none");
    }
  std::cout << "DFA_REDUCE_NARY_WIDTH_MAX invalid value rejected: passed" << std::endl;
  std::cout.flush();
}

// _reduce_nary's greedy batching (see its own comment) combines up to
// width_max currently-cheapest operands per round, feeding each result
// back in for further rounds until one remains -- both
// test_reduce_nary_chunking and test_reduce_nary_width_max_override stay
// within a single round (40 operands at the default cap of 16, or 10 at an
// override of 4), so neither exercises more than one merge. Force several
// rounds here instead: a small enough override relative to the operand
// count needs repeated rounds before anything is small enough to finish,
// checked against the same pairwise fold used everywhere else in this
// file.
void test_reduce_nary_deep_levels(const dfa_shape_t& shape)
{
  std::cout << "checking DFA_REDUCE_NARY_WIDTH_MAX deep recursion" << std::endl;
  std::cout.flush();

  std::vector<DFAString> positions = get_all_positions(shape);

  const size_t width = 40; // well over the override below (3), several merge rounds
  assert(width <= positions.size());

  std::vector<shared_dfa_ptr> singletons;
  for(size_t i = 0; i < width; ++i)
    {
      singletons.push_back(shared_dfa_ptr(new StringDFA(shape, std::vector<DFAString>({positions[i]}))));
    }

  setenv("DFA_REDUCE_NARY_WIDTH_MAX", "3", 1);
  shared_dfa_ptr union_actual = DFAUtil::get_union_vector(shape, singletons);
  unsetenv("DFA_REDUCE_NARY_WIDTH_MAX");

  shared_dfa_ptr union_expected = singletons[0];
  for(size_t i = 1; i < singletons.size(); ++i)
    {
      union_expected = shared_dfa_ptr(new UnionDFA(*union_expected, *singletons[i]));
    }
  if(!DFAUtil::get_difference(union_actual, union_expected)->is_constant(false) ||
     !DFAUtil::get_difference(union_expected, union_actual)->is_constant(false))
    {
      throw std::logic_error("DFA_REDUCE_NARY_WIDTH_MAX=3 deep recursion: union language mismatch vs pairwise reduction");
    }
  test_helper("DFA_REDUCE_NARY_WIDTH_MAX=3 deep recursion union", *union_actual, width);
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

  // vector (n-ary) union/intersection tests -- the BinaryDFA constructor
  // meant to replace get_intersection_vector/get_union_vector's pairwise
  // reduction. size 1, and duplicate entries, exercise n=1 and idempotence
  // directly; the rest reuse the disjointness/overlap facts the pairwise
  // tests above already established for count0..count3, zero0 and one1.

  test_intersection_vector("count2 x1", std::vector<shared_dfa_ptr>({count2}), size_t(count2->size()));
  test_intersection_vector("count1 x3 (idempotent)", std::vector<shared_dfa_ptr>({count1, count1, count1}), size_t(count1->size()));
  test_intersection_vector("count0+count1+count2 (disjoint)", std::vector<shared_dfa_ptr>({count0, count1, count2}), size_t(0));
  test_intersection_vector("count2+count2+count3 (disjoint)", std::vector<shared_dfa_ptr>({count2, count2, count3}), size_t(0));
  test_intersection_vector("zero0+count1+one1", std::vector<shared_dfa_ptr>({zero0, count1, one1}), size_t(0));

  test_union_vector("count2 x1", std::vector<shared_dfa_ptr>({count2}), size_t(count2->size()));
  test_union_vector("count1 x3 (idempotent)", std::vector<shared_dfa_ptr>({count1, count1, count1}), size_t(count1->size()));
  test_union_vector("count0+count1+count2+count3 (disjoint)",
                    std::vector<shared_dfa_ptr>({count0, count1, count2, count3}),
                    size_t(count0->size() + count1->size() + count2->size() + count3->size()));
  test_union_vector("count1+count1+count2", std::vector<shared_dfa_ptr>({count1, count1, count2}), size_t(count1->size() + count2->size()));
  test_union_vector("accept+count1", std::vector<shared_dfa_ptr>({accept, count1}), accept_expected);
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

      test_linear_bound();
      test_reduce_nary_chunking(dfa_shape_t({TEST5_DFA_SHAPE}));
      test_reduce_nary_width_max_override(dfa_shape_t({TEST5_DFA_SHAPE}));
      test_reduce_nary_deep_levels(dfa_shape_t({TEST5_DFA_SHAPE}));
    }
  catch(const std::logic_error& e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
