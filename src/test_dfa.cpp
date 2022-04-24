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
#include "TicTacToeGame.h"
#include "UnionDFA.h"

template<int ndim, int... shape_pack>
std::string get_parameter_string()
{
  int shape[] = {shape_pack...};

  std::ostringstream builder;
  builder << ndim << "=" << shape[0];
  for(int layer = 1; layer < ndim; ++layer)
    {
      builder << "/" << shape[layer];
    }

  return builder.str();
}

template<int ndim, int... shape_pack>
void test_union_pair(std::string test_name, const DFA<ndim, shape_pack...>& left, const DFA<ndim, shape_pack...>& right, int expected_boards);

template<int ndim, int... shape_pack>
void test_helper(std::string test_name, const DFA<ndim, shape_pack...>& test_dfa, int expected_boards)
{
  int actual_boards = test_dfa.size();
  if(actual_boards != expected_boards)
    {
      std::cerr << get_parameter_string<ndim, shape_pack...>() << " " << test_name << ": expected " << expected_boards << std::endl;
      std::cerr << get_parameter_string<ndim, shape_pack...>() << " " << test_name << ":   actual " << actual_boards << std::endl;

      throw std::logic_error(test_name + ": test failed");
    }
  std::cout << get_parameter_string<ndim, shape_pack...>() << " " << test_name << ": passed" << std::endl;
  std::cout.flush();
}

template<int ndim, int... shape_pack>
void test_intersection_pair(std::string test_name, const DFA<ndim, shape_pack...>& left, const DFA<ndim, shape_pack...>& right, int expected_boards)
{
  std::cout << "checking intersection pair " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA<ndim, shape_pack...> test_dfa(left, right);
  test_helper("intersection pair " + test_name, test_dfa, expected_boards);
}

template<int ndim, int... shape_pack>
void test_intersection_vector(std::string test_name, const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>> dfas_in, int expected_boards)
{
  std::cout << "checking intersection vector " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA<ndim, shape_pack...> test_dfa(dfas_in);
  test_helper("intersection vector " + test_name, test_dfa, expected_boards);
}

template<int ndim, int... shape_pack>
void test_inverse(std::string test_name, const DFA<ndim, shape_pack...>& dfa_in)
{
  std::cout << "checking inverse " << test_name << std::endl;
  std::cout.flush();

  int shape[] = {shape_pack...};
  int accept_all_boards = 1;
  for(int layer = 0; layer < ndim; ++layer)
    {
      accept_all_boards *= shape[layer];
    }
  int expected_boards = accept_all_boards - dfa_in.size();

  InverseDFA<ndim, shape_pack...> test_dfa(dfa_in);
  test_helper("inverse " + test_name, test_dfa, expected_boards);

  IntersectionDFA<ndim, shape_pack...> intersection_dfa(test_dfa, dfa_in);
  test_intersection_pair("inverse " + test_name, test_dfa, dfa_in, 0);

  test_union_pair("inverse union " + test_name, test_dfa, dfa_in, accept_all_boards);
}

template<int ndim, int... shape_pack>
void test_union_pair(std::string test_name, const DFA<ndim, shape_pack...>& left, const DFA<ndim, shape_pack...>& right, int expected_boards)
{
  std::cout << "checking union pair " << test_name << std::endl;
  std::cout.flush();

  UnionDFA<ndim, shape_pack...> test_dfa(left, right);
  test_helper("union pair " + test_name, test_dfa, expected_boards);
}

template<int ndim, int... shape_pack>
void test_union_vector(std::string test_name, const std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>> dfas_in, int expected_boards)
{
  std::cout << "checking union vector " << test_name << std::endl;
  std::cout.flush();

  UnionDFA<ndim, shape_pack...> test_dfa(dfas_in);
  test_helper("union vector " + test_name, test_dfa, expected_boards);
}

template<int ndim, int... shape_pack>
void test_suite()
{
  const int shape[] = {shape_pack...};

  // accept all

  std::shared_ptr<const DFA<ndim, shape_pack...>> accept(new AcceptDFA<ndim, shape_pack...>());
  size_t accept_expected = 1;
  for(int layer = 0; layer < ndim; ++layer)
    {
      accept_expected *= shape[layer];
    }
  test_helper("accept", *accept, accept_expected);

  // reject all

  std::shared_ptr<const DFA<ndim, shape_pack...>> reject(new RejectDFA<ndim, shape_pack...>());
  test_helper("reject", *reject, 0);

  // count tests

  std::shared_ptr<const DFA<ndim, shape_pack...>> count0(new CountDFA<ndim, shape_pack...>(0));
  test_helper("count0", *count0, 1);

  std::shared_ptr<const DFA<ndim, shape_pack...>> count1(new CountDFA<ndim, shape_pack...>(1));
  size_t count1_expected = 0;
  for(int layer = 0; layer < ndim; ++layer)
    {
      count1_expected += shape[layer] - 1;
    }
  test_helper("count1", *count1, count1_expected);

  std::shared_ptr<const DFA<ndim, shape_pack...>> count2(new CountDFA<ndim, shape_pack...>(2));
  size_t count2_expected = 0;
  for(int l1 = 0; l1 < ndim; ++l1)
    {
      for(int l2 = l1 + 1; l2 < ndim; ++l2)
	{
	  count2_expected += (shape[l1] - 1) * (shape[l2] - 1);
	}
    }
  test_helper("count2", *count2, count2_expected);

  std::shared_ptr<const DFA<ndim, shape_pack...>> count3(new CountDFA<ndim, shape_pack...>(3));
  size_t count3_expected = 0;
  for(int l1 = 0; l1 < ndim; ++l1)
    {
      for(int l2 = l1 + 1; l2 < ndim; ++l2)
	{
	  for(int l3 = l2 + 1; l3 < ndim; ++l3)
	    {
	      count3_expected += (shape[l1] - 1) * (shape[l2] - 1) * (shape[l3] - 1);
	    }
	}
    }
  test_helper("count3", *count3, count3_expected);

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

  test_union_vector("count0", std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>({count0}), count0->size());
  test_union_vector("count0+count1", std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>({count0, count1}), count0->size() + count1->size());
  test_union_vector("count0+count1+count2", std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>({count0, count1, count2}), count0->size() + count1->size() + count2->size());
  test_union_vector("count0+count1+count2+count3", std::vector<std::shared_ptr<const DFA<ndim, shape_pack...>>>({count0, count1, count2, count3}), count0->size() + count1->size() + count2->size() + count3->size());

  // inverse tests

  test_inverse("accept", *accept);
  test_inverse("reject", *reject);
  test_inverse("count0", *count0);
  test_inverse("count1", *count1);
  test_inverse("count2", *count2);
  test_inverse("count3", *count3);

  // count character tests

  std::shared_ptr<const DFA<ndim, shape_pack...>> zero0(new CountCharacterDFA<ndim, shape_pack...>(0, 0));
  size_t zero0_expected = 1;
  for(int layer = 0; layer < ndim; ++layer)
    {
      zero0_expected *= shape[layer] - 1;
    }
  test_helper("zero0", *zero0, zero0_expected);

  std::shared_ptr<const DFA<ndim, shape_pack...>> zero1(new CountCharacterDFA<ndim, shape_pack...>(0, 1));
  size_t zero1_expected = 0;
  for(int l0 = 0; l0 < ndim; ++l0)
    {
      size_t l0_expected = 1;
      for(int lo = 0; lo < ndim; ++lo)
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

  std::shared_ptr<const DFA<ndim, shape_pack...>> one1(new CountCharacterDFA<ndim, shape_pack...>(1, 1));
  size_t one1_expected = 0;
  for(int l1 = 0; l1 < ndim; ++l1)
    {
      if(shape[l1] < 2)
	{
	  // can't be one at this layer
	  continue;
	}

      size_t l1_expected = 1;
      for(int lo = 0; lo < ndim; ++lo)
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
  for(int layer = 0; layer < ndim; ++layer)
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
      test_suite<TEST4_DFA_PARAMS>();
      test_suite<TEST5_DFA_PARAMS>();
      test_suite<TICTACTOE2_DFA_PARAMS>();
      test_suite<TICTACTOE3_DFA_PARAMS>();
      test_suite<TICTACTOE4_DFA_PARAMS>();
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
