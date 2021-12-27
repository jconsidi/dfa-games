// test_union_dfa.cpp

#include <iostream>
#include <string>

#include "CountDFA.h"
#include "UnionDFA.h"

void test(std::string test_name, const DFA& left, const DFA& right, uint64_t expected_boards)
{
  std::cout << "checking " << test_name << std::endl;
  std::cout.flush();
  
  UnionDFA test_dfa(left, right);
  
  uint64_t actual_boards = test_dfa.size();
  std::cout << left.size() << " + " << right.size() << " => " << actual_boards << std::endl;
  std::cout.flush();

  if(actual_boards != expected_boards)
    {
      left.debug_counts("left");
      right.debug_counts("right");
      test_dfa.debug_counts("test_dfa");

      std::cerr << test_name << ": expected " << expected_boards << std::endl;
      std::cerr << test_name << ":   actual " << actual_boards << std::endl;
      
      throw std::logic_error("UnionDFA construction failed");
    }
}

int main()
{
  CountDFA count0(0);
  CountDFA count1(1);
  CountDFA count2(2);
  CountDFA count3(3);

  try
    {
      test("count0+count0", count0, count0, count0.size());
      test("count0+count1", count0, count1, count0.size() + count1.size());
      test("count1+count0", count1, count0, count0.size() + count1.size());
      test("count1+count1", count1, count1, count1.size());
      test("count2+count3", count2, count3, count2.size() + count3.size());
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }
  
  return 0;
}
