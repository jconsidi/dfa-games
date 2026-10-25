// test_sort_join_faults.cpp
//
// Builds a union on the backward pass's sort join with one fault injected,
// and checks the join throws naming it. A build abandoned by an exception
// leaves later DFA construction in the process asserting (see DFA.cpp), so
// each fault runs in its own process:
//
//   test_sort_join_faults none            no fault; the build must succeed
//   test_sort_join_faults next_pair_hole  next pair rank 1 reads as (0, 0)
//   test_sort_join_faults repeated_index  transition 1 carries index 0
//   test_sort_join_faults unreached_pairs every transition short circuits

#include <iostream>
#include <stdexcept>
#include <string>

#include "BinaryDFA.h"
#include "CountDFA.h"
#include "UnionDFA.h"

int main(int argc, char **argv)
{
  if(argc != 2)
    {
      std::cerr << "usage: " << argv[0] << " none|next_pair_hole|repeated_index|unreached_pairs" << std::endl;
      return 2;
    }
  std::string fault(argv[1]);

  // The message the join must throw, or empty for no throw.
  std::string expected;
  if(fault == "none")
    {
    }
  else if(fault == "next_pair_hole")
    {
      binary_dfa_sort_join_next_pair_fault = [](size_t rank, uint64_t& left, uint64_t& right)
      {
        if(rank == 1)
          {
            left = 0;
            right = 0;
          }
      };
      expected = "pair rank 1 is (0, 0)";
    }
  else if(fault == "repeated_index")
    {
      binary_dfa_sort_join_lookup_fault = [](size_t transition_index, uint64_t&, uint64_t&, uint64_t& index)
      {
        if(transition_index == 1)
          {
            index = 0;
          }
      };
      expected = "so transition 1 is missing or repeated";
    }
  else if(fault == "unreached_pairs")
    {
      binary_dfa_sort_join_lookup_fault = [](size_t, uint64_t& left, uint64_t& right, uint64_t&)
      {
        left = 0;
        right = 0;
      };
      expected = "transitions reach 0 distinct pairs of layer";
    }
  else
    {
      std::cerr << "unknown fault " << fault << std::endl;
      return 2;
    }

  binary_dfa_sort_join = BinaryDFASortJoin::always;

  dfa_shape_t shape({2, 2, 2, 2, 2, 2});
  std::string name = "6=2/2/2/2/2/2 sort join fault " + fault;
  CountDFA count1(shape, 1);
  CountDFA count2(shape, 2);

  try
    {
      UnionDFA joined(count1, count2);
      if(!expected.empty())
        {
          std::cerr << name << ": built " << joined.states() << " states, expected a throw containing \"" << expected << "\"" << std::endl;
          return 1;
        }
      if(binary_dfa_sort_join_layers == 0)
        {
          std::cerr << name << ": the build never used the sort join" << std::endl;
          return 1;
        }
      std::cout << name << ": passed (" << joined.states() << " states, " << binary_dfa_sort_join_layers << " backward layers by the join)" << std::endl;
      return 0;
    }
  catch(const std::exception& e)
    {
      std::string message(e.what());
      if(expected.empty() || (message.find(expected) == std::string::npos))
        {
          std::cerr << name << ": threw \"" << message << "\"" << (expected.empty() ? std::string() : ", expected it to contain \"" + expected + "\"") << std::endl;
          return 1;
        }
      std::cout << name << ": passed (threw \"" << message << "\")" << std::endl;
      return 0;
    }
}
