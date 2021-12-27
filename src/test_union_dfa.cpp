// test_union_dfa.cpp

#include <iostream>
#include <string>

#include "CountDFA.h"
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

void test_pair(std::string test_name, const DFA& left, const DFA& right, DFA::size_type expected_boards)
{
  std::cout << "checking " << test_name << std::endl;
  std::cout.flush();

  UnionDFA test_dfa(left, right);
  test_helper(test_name, test_dfa, expected_boards);
}

void test_vector(std::string test_name, const std::vector<const DFA *> dfas_in, DFA::size_type expected_boards)
{
  std::cout << "checking " << test_name << std::endl;
  std::cout.flush();

  UnionDFA test_dfa(dfas_in);
  test_helper(test_name, test_dfa, expected_boards);
}

int main()
{
  CountDFA count0(0);
  CountDFA count1(1);
  CountDFA count2(2);
  CountDFA count3(3);

  try
    {
      test_pair("pair count0+count0", count0, count0, count0.size());
      test_pair("pair count0+count1", count0, count1, count0.size() + count1.size());
      test_pair("pair count1+count0", count1, count0, count0.size() + count1.size());
      test_pair("pair count1+count1", count1, count1, count1.size());
      test_pair("pair count2+count3", count2, count3, count2.size() + count3.size());

      test_vector("vector count0", std::vector<const DFA *>({&count0}), count0.size());
      test_vector("vector count0+count1", std::vector<const DFA *>({&count0, &count1}), count0.size() + count1.size());
      test_vector("vector count0+count1+count2", std::vector<const DFA *>({&count0, &count1, &count2}), count0.size() + count1.size() + count2.size());
      test_vector("vector count0+count1+count2+count3", std::vector<const DFA *>({&count0, &count1, &count2, &count3}), count0.size() + count1.size() + count2.size() + count3.size());
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }
  
  return 0;
}
