// test_union_dfa.cpp

#include <iostream>
#include <string>

#include "AcceptDFA.h"
#include "CountCharacterDFA.h"
#include "CountDFA.h"
#include "DFA.h"
#include "IntersectionDFA.h"
#include "InverseDFA.h"
#include "RejectDFA.h"
#include "UnionDFA.h"

typedef std::shared_ptr<const TestDFA> shared_dfa_ptr;

void test_helper(std::string test_name, const TestDFA& test_dfa, int expected_boards)
{
  int actual_boards = test_dfa.size();
  if(actual_boards != expected_boards)
    {
      std::cerr << test_name << ": expected " << expected_boards << std::endl;
      std::cerr << test_name << ":   actual " << actual_boards << std::endl;

      throw std::logic_error(test_name + ": test failed");
    }
  std::cout << test_name << ": passed" << std::endl;
  std::cout.flush();
}

void test_intersection_pair(std::string test_name, const TestDFA& left, const TestDFA& right, int expected_boards)
{
  std::cout << "checking intersection pair " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA<TEST_DFA_PARAMS> test_dfa(left, right);
  test_helper("intersection pair " + test_name, test_dfa, expected_boards);
}

void test_intersection_vector(std::string test_name, const std::vector<std::shared_ptr<const TestDFA>> dfas_in, int expected_boards)
{
  std::cout << "checking intersection vector " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA<TEST_DFA_PARAMS> test_dfa(dfas_in);
  test_helper("intersection vector " + test_name, test_dfa, expected_boards);
}

void test_inverse(std::string test_name, const TestDFA& dfa_in)
{
  std::cout << "checking inverse " << test_name << std::endl;
  std::cout.flush();

  int expected_boards = 24 - dfa_in.size();

  InverseDFA<TEST_DFA_PARAMS> test_dfa(dfa_in);
  test_helper("inverse " + test_name, test_dfa, expected_boards);

  IntersectionDFA<TEST_DFA_PARAMS> intersection_dfa(test_dfa, dfa_in);
  test_intersection_pair("inverse " + test_name, test_dfa, dfa_in, 0);
}

void test_union_pair(std::string test_name, const TestDFA& left, const TestDFA& right, int expected_boards)
{
  std::cout << "checking union pair " << test_name << std::endl;
  std::cout.flush();

  UnionDFA<TEST_DFA_PARAMS> test_dfa(left, right);
  test_helper("union pair " + test_name, test_dfa, expected_boards);
}

void test_union_vector(std::string test_name, const std::vector<shared_dfa_ptr> dfas_in, int expected_boards)
{
  std::cout << "checking union vector " << test_name << std::endl;
  std::cout.flush();

  UnionDFA<TEST_DFA_PARAMS> test_dfa(dfas_in);
  test_helper("union vector " + test_name, test_dfa, expected_boards);
}

int main()
{
  try
    {
      // accept all

      shared_dfa_ptr accept(new AcceptDFA<TEST_DFA_PARAMS>());
      test_helper("accept", *accept, 24); // 1 * 2 * 3 * 4

      // reject all

      shared_dfa_ptr reject(new RejectDFA<TEST_DFA_PARAMS>());
      test_helper("reject", *reject, 0);

      // count tests

      shared_dfa_ptr count0(new CountDFA<TEST_DFA_PARAMS>(0));
      test_helper("count0", *count0, 1);

      shared_dfa_ptr count1(new CountDFA<TEST_DFA_PARAMS>(1));
      test_helper("count1", *count1, 0 + 1 + 2 + 3);

      shared_dfa_ptr count2(new CountDFA<TEST_DFA_PARAMS>(2));
      test_helper("count2", *count2, 1 * 2 + 1 * 3 + 2 * 3);

      shared_dfa_ptr count3(new CountDFA<TEST_DFA_PARAMS>(3));
      test_helper("count3", *count3, 1 * 1 * 2 * 3);

      // intersection tests

      test_intersection_pair("count0+count0", *count0, *count0, count0->size());
      test_intersection_pair("count0+count1", *count0, *count1, 0);
      test_intersection_pair("count1+count1", *count1, *count1, count1->size());
      test_intersection_pair("count2+count3", *count2, *count3, 0);

      // union tests

      test_union_pair("count0+count0", *count0, *count0, count0->size());
      test_union_pair("count0+count1", *count0, *count1, count0->size() + count1->size());
      test_union_pair("count1+count0", *count1, *count0, count0->size() + count1->size());
      test_union_pair("count1+count1", *count1, *count1, count1->size());
      test_union_pair("count2+count3", *count2, *count3, count2->size() + count3->size());

      test_union_vector("count0", std::vector<shared_dfa_ptr>({count0}), count0->size());
      test_union_vector("count0+count1", std::vector<shared_dfa_ptr>({count0, count1}), count0->size() + count1->size());
      test_union_vector("count0+count1+count2", std::vector<shared_dfa_ptr>({count0, count1, count2}), count0->size() + count1->size() + count2->size());
      test_union_vector("count0+count1+count2+count3", std::vector<shared_dfa_ptr>({count0, count1, count2, count3}), count0->size() + count1->size() + count2->size() + count3->size());

      // inverse tests

      test_inverse("accept", *accept);
      test_inverse("reject", *reject);
      test_inverse("count0", *count0);
      test_inverse("count1", *count1);
      test_inverse("count2", *count2);
      test_inverse("count3", *count3);

      // count character tests

      shared_dfa_ptr zero0(new CountCharacterDFA<TEST_DFA_PARAMS>(0, 0));
      test_helper("zero0", *zero0, 0);

      shared_dfa_ptr zero1(new CountCharacterDFA<TEST_DFA_PARAMS>(0, 1));
      test_helper("zero1", *zero1, 1 * 1 * 2 * 3);
      test_intersection_pair("zero1 + count1", *zero0, *count1, 0);

      shared_dfa_ptr one1(new CountCharacterDFA<TEST_DFA_PARAMS>(1, 1));
      test_helper("one1", *one1, 1 * 2 * 3 +  1 * 1 * 3 + 1 * 2 * 1);
      test_intersection_pair("one1 + count1", *one1, *count1, 3);
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
