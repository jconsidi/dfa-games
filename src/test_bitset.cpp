// test_bitset.cpp

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "BitSet.h"

typedef std::vector<uint64_t> vector;

void test_iterator(const BitSet& set, const vector& expected)
{
  auto iter = set.cbegin();
  auto iter_end = set.cend();

  for(int i = 0; i < expected.size(); ++i)
    {
      assert(iter < iter_end);
      assert(*iter == expected[i]);
      ++iter;
    }

  assert(!(iter < iter_end));
}

void test_vector(size_t size, const vector& inputs, const vector& expected)
{
  BitSet set(size);
  populate_bitset<decltype(inputs.cbegin())>(set, inputs.cbegin(), inputs.cend());

  test_iterator(set, expected);
}

void test_vector(size_t size, const vector& inputs)
{
  test_vector(size, inputs, inputs);
}

int main()
{
  try
    {
      test_vector(1024, vector({0}));
      test_vector(1024, vector({63}));
      test_vector(1024, vector({64}));
      test_vector(1024, vector({127}));

      test_vector(1024, vector({0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66}));

      // big sparse examples
      test_vector(1ULL << 23, vector({(1ULL << 20) + 4096, (1ULL << 20) + 32767, 1ULL << 22}));
      test_vector(1ULL << 37, vector({0, 1ULL << 20, 1ULL << 30, 1ULL << 36}));
    }
  catch(const std::logic_error& e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
