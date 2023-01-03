// test_union_dfa.cpp

#include <iostream>
#include <sstream>
#include <string>

#include "AcceptDFA.h"
#include "CountCharacterDFA.h"
#include "CountDFA.h"
#include "DFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "RejectDFA.h"
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

void test_union_pair(std::string test_name, const DFA& left, const DFA& right, int expected_boards);

void test_helper(std::string test_name, const DFA& test_dfa, int expected_boards)
{
  int actual_boards = test_dfa.size();
  if(actual_boards != expected_boards)
    {
      std::cerr << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ": expected " << expected_boards << std::endl;
      std::cerr << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ":   actual " << actual_boards << std::endl;

      throw std::logic_error(test_name + ": test failed");
    }
  std::cout << get_parameter_string(test_dfa.get_shape()) << " " << test_name << ": passed" << std::endl;
  std::cout.flush();
}

void test_intersection_pair(std::string test_name, const DFA& left, const DFA& right, int expected_boards)
{
  std::cout << "checking intersection pair " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA test_dfa(left, right);
  test_helper("intersection pair " + test_name, test_dfa, expected_boards);
}

void test_inverse(std::string test_name, const DFA& dfa_in)
{
  std::cout << "checking inverse " << test_name << std::endl;
  std::cout.flush();

  int accept_all_boards = 1;
  for(auto layer_shape : dfa_in.get_shape())
    {
      accept_all_boards *= layer_shape;
    }
  int expected_boards = accept_all_boards - dfa_in.size();

  InverseDFA test_dfa(dfa_in);
  test_helper("inverse " + test_name, test_dfa, expected_boards);

  IntersectionDFA intersection_dfa(test_dfa, dfa_in);
  test_intersection_pair("inverse " + test_name, test_dfa, dfa_in, 0);

  test_union_pair("inverse union " + test_name, test_dfa, dfa_in, accept_all_boards);
}

void test_union_pair(std::string test_name, const DFA& left, const DFA& right, int expected_boards)
{
  std::cout << "checking union pair " << test_name << std::endl;
  std::cout.flush();

  UnionDFA test_dfa(left, right);
  test_helper("union pair " + test_name, test_dfa, expected_boards);
}

void test_suite(const dfa_shape_t& shape)
{

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
  save->save("test");
  save = 0; // trigger destructor

  std::shared_ptr<const DFA> load(new DFA(shape, "test"));
  test_helper("load", *load, 1);

  std::shared_ptr<DFA> save2(new AcceptDFA(shape));
  // overwrite save
  save2->save("test");

  // intersection tests

  test_intersection_pair("count0+count0", *count0, *count0, count0->size());
  test_intersection_pair("count0+count1", *count0, *count1, 0);
  test_intersection_pair("count1+count1", *count1, *count1, count1->size());
  test_intersection_pair("count1+count2", *count1, *count2, 0);
  test_intersection_pair("count2+count1", *count2, *count1, 0);
  test_intersection_pair("count2+count2", *count2, *count2, count2->size());
  test_intersection_pair("count2+count3", *count2, *count3, 0);
  test_intersection_pair("count3+count2", *count3, *count2, 0);
  test_intersection_pair("count3+count3", *count3, *count3, count3->size());

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
  test_intersection_pair("zero1 + count1", *zero0, *count1, 0);

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

int main()
{
  try
    {
      test_suite(dfa_shape_t({TEST1_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST2_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST3_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST4_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST5_DFA_SHAPE}));
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
