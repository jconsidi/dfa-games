// test_sort_unique.cpp

#include <cassert>
#include <cstdlib>
#include <vector>

#include "sort.h"

void test(const std::vector<size_t>& input, size_t expected_length)
{
  std::vector<size_t> expected(input);
  std::sort(expected.begin(), expected.end());
  auto expected_end = std::unique(expected.begin(), expected.end());
  assert(expected_end - expected.begin() == expected_length);
  expected.resize(expected_length);

  std::vector<size_t> test_copy(input);
  auto test_end = sort_unique<size_t>(&*test_copy.begin(), &*test_copy.end());
  size_t test_length = test_end - &*test_copy.begin();
  assert(test_length == expected_length);

  for(size_t i = 0; i < expected_length; ++i)
    {
      assert(test_copy.at(i) == expected.at(i));
    }
}

int main()
{
  std::vector<size_t> test_0;
  test(test_0, 0);

  std::vector<size_t> test_1({42});
  test(test_1, 1);

  std::vector<size_t> test_2({0, 1, 2, 3});
  test(test_2, 4);

  std::vector<size_t> test_3({0, 0, 1, 1, 2, 2, 3, 3});
  test(test_3, 4);

  std::vector<size_t> test_4(1ULL << 22, 0);
  test(test_4, 1);

  std::vector<size_t> test_5;
  for(int i = 0; i < 1ULL << 20; ++i)
    {
      test_5.push_back(0);
      test_5.push_back(1);
      test_5.push_back(2);
      test_5.push_back(3);
    }
  test(test_5, 4);

  return 0;
}
