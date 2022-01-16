// test_union_dfa.cpp

#include <iostream>
#include <string>

#include "AcceptDFA.h"
#include "CountCharacterDFA.h"
#include "CountDFA.h"
#include "DFA.h"
#include "IntersectionDFA.h"
#include "RejectDFA.h"
#include "UnionDFA.h"

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

void test_intersection_vector(std::string test_name, const std::vector<const TestDFA *> dfas_in, int expected_boards)
{
  std::cout << "checking intersection vector " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA<TEST_DFA_PARAMS> test_dfa(dfas_in);
  test_helper("intersection vector " + test_name, test_dfa, expected_boards);
}

void test_union_pair(std::string test_name, const TestDFA& left, const TestDFA& right, int expected_boards)
{
  std::cout << "checking union pair " << test_name << std::endl;
  std::cout.flush();

  UnionDFA<TEST_DFA_PARAMS> test_dfa(left, right);
  test_helper("union pair " + test_name, test_dfa, expected_boards);
}

void test_union_vector(std::string test_name, const std::vector<const TestDFA *> dfas_in, int expected_boards)
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

      AcceptDFA<TEST_DFA_PARAMS> accept;
      test_helper("accept", accept, 24); // 1 * 2 * 3 * 4

      // reject all

      RejectDFA<TEST_DFA_PARAMS> reject;
      test_helper("reject", reject, 0);

      // count tests

      CountDFA<TEST_DFA_PARAMS> count0(0);
      test_helper("count0", count0, 1);

      CountDFA<TEST_DFA_PARAMS> count1(1);
      test_helper("count1", count1, 0 + 1 + 2 + 3);

      CountDFA<TEST_DFA_PARAMS> count2(2);
      test_helper("count2", count2, 1 * 2 + 1 * 3 + 2 * 3);

      CountDFA<TEST_DFA_PARAMS> count3(3);
      test_helper("count3", count3, 1 * 1 * 2 * 3);

      // intersection tests

      test_intersection_pair("count0+count0", count0, count0, count0.size());
      test_intersection_pair("count0+count1", count0, count1, 0);
      test_intersection_pair("count1+count1", count1, count1, count1.size());
      test_intersection_pair("count2+count3", count2, count3, 0);

      // union tests

      test_union_pair("count0+count0", count0, count0, count0.size());
      test_union_pair("count0+count1", count0, count1, count0.size() + count1.size());
      test_union_pair("count1+count0", count1, count0, count0.size() + count1.size());
      test_union_pair("count1+count1", count1, count1, count1.size());
      test_union_pair("count2+count3", count2, count3, count2.size() + count3.size());

      test_union_vector("count0", std::vector<const TestDFA *>({&count0}), count0.size());
      test_union_vector("count0+count1", std::vector<const TestDFA *>({&count0, &count1}), count0.size() + count1.size());
      test_union_vector("count0+count1+count2", std::vector<const TestDFA *>({&count0, &count1, &count2}), count0.size() + count1.size() + count2.size());
      test_union_vector("count0+count1+count2+count3", std::vector<const TestDFA *>({&count0, &count1, &count2, &count3}), count0.size() + count1.size() + count2.size() + count3.size());

      // count character tests

      CountCharacterDFA<TEST_DFA_PARAMS> zero0(0, 0);
      test_helper("zero0", zero0, 0);

      CountCharacterDFA<TEST_DFA_PARAMS> zero1(0, 1);
      test_helper("zero1", zero1, 1 * 1 * 2 * 3);
      test_intersection_pair("zero1 + count1", zero0, count1, 0);

      CountCharacterDFA<TEST_DFA_PARAMS> one1(1, 1);
      test_helper("one1", one1, 1 * 2 * 3 +  1 * 1 * 3 + 1 * 2 * 1);
      test_intersection_pair("one1 + count1", one1, count1, 3);
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
