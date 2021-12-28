// test_count_dfa.cpp

#include <iostream>

#include "CountDFA.h"

void test(int num_pieces, DFA::size_type expected_boards)
{
  std::cout << "checking " << num_pieces << " pieces" << std::endl;
  std::cout.flush();

  CountDFA test_dfa(num_pieces);
  DFA::size_type actual_boards = test_dfa.size();

  if(actual_boards != expected_boards)
    {
      std::cerr << "num_pieces = " << num_pieces << ": expected " << expected_boards << std::endl;
      std::cerr << "num_pieces = " << num_pieces << ":   actual " << actual_boards << std::endl;

      throw std::logic_error("CountDFA construction failed");
    }

  std::cout << " used " << test_dfa.states() << " states" << std::endl;
}

int main()
{
  try
    {
      test(0, 1);
      test(1, 12 * 64);
      test(2, (12 * 12) * (64 * 63 / 2));
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }

  return 0;
}
