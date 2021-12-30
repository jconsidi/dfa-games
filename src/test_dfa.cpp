// test_union_dfa.cpp

#include <iostream>
#include <string>

#include "CountDFA.h"
#include "IntersectionDFA.h"
#include "UnionDFA.h"

void test_helper(std::string test_name, const DFA& test_dfa, DFA::size_type expected_boards)
{
  DFA::size_type actual_boards = test_dfa.size();
  if(actual_boards != expected_boards)
    {
      test_dfa.debug_counts("test_dfa");

      std::cerr << test_name << ": expected " << expected_boards << std::endl;
      std::cerr << test_name << ":   actual " << actual_boards << std::endl;

      throw std::logic_error("UnionDFA construction failed");
    }
}

void test_intersection_pair(std::string test_name, const DFA& left, const DFA& right, DFA::size_type expected_boards)
{
  std::cout << "checking intersection pair " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA test_dfa(left, right);
  test_helper("intersection pair " + test_name, test_dfa, expected_boards);
}

void test_union_pair(std::string test_name, const DFA& left, const DFA& right, DFA::size_type expected_boards)
{
  std::cout << "checking union pair " << test_name << std::endl;
  std::cout.flush();

  UnionDFA test_dfa(left, right);
  test_helper("union pair " + test_name, test_dfa, expected_boards);
}

void test_union_vector(std::string test_name, const std::vector<const DFA *> dfas_in, DFA::size_type expected_boards)
{
  std::cout << "checking union vector " << test_name << std::endl;
  std::cout.flush();

  UnionDFA test_dfa(dfas_in);
  test_helper("union vector " + test_name, test_dfa, expected_boards);
}

int main()
{
  try
    {
      // count tests
      
      CountDFA count0(0);
      test_helper("count0", count0, 1);

      CountDFA count1(1);
      test_helper("count1", count1, 12 * 64);

      CountDFA count2(2);
      test_helper("count2", count2, (12 * 12) * (64 * 63 / 2));

      CountDFA count3(3);
      test_helper("count3", count3, (12 * 12 * 12) * (64 * 63 * 62 / 3 / 2));

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

      test_union_vector("count0", std::vector<const DFA *>({&count0}), count0.size());
      test_union_vector("count0+count1", std::vector<const DFA *>({&count0, &count1}), count0.size() + count1.size());
      test_union_vector("count0+count1+count2", std::vector<const DFA *>({&count0, &count1, &count2}), count0.size() + count1.size() + count2.size());
      test_union_vector("count0+count1+count2+count3", std::vector<const DFA *>({&count0, &count1, &count2, &count3}), count0.size() + count1.size() + count2.size() + count3.size());
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }

  return 0;
}
