// sort.cpp

// sort wrappers to go parallel when possible, but limit working set
// size since std::sort may blow out RAM.

#include "sort.h"

#include <algorithm>
#include <cassert>
#include <execution>
#include <iostream>

#include "Flashsort.h"

#ifdef __cpp_lib_parallel_algorithm
#define TRY_PARALLEL_2(f, a, b) f(std::execution::par_unseq, a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(std::execution::par_unseq, a, b, c)
#define TRY_PARALLEL_4(f, a, b, c, d) f(std::execution::par_unseq, a, b, c, d)
#define TRY_PARALLEL_5(f, a, b, c, d, e) f(std::execution::par_unseq, a, b, c, d, e)
#else
#define TRY_PARALLEL_2(f, a, b) f(a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(a, b, c)
#define TRY_PARALLEL_4(f, a, b, c, d) f(a, b, c, d)
#define TRY_PARALLEL_5(f, a, b, c, d, e) f(a, b, c, d, e)
#endif

#define SORT_MAX_PARALLEL_BYTES (size_t(1) << 32)

template <class T>
void sort(T *begin, T *end)
{
#ifdef __cpp_lib_parallel_algorithm
  size_t size_elements = (end - begin);
  size_t size_bytes = size_elements * sizeof(T);
  if((size_bytes >= SORT_MAX_PARALLEL_BYTES) && (size_elements >= 2))
    {
      // divide and conquer so extra working space doesn't exhaust
      // RAM. this has happened several times while sorting memory
      // mapped files close to 2x RAM.
      T *mid = begin + size_elements / 2;

      // parallel policy is allowed O(n log n) swaps, and uncertain
      // about working space, so stay serial.
      std::nth_element(begin, mid, end);

      sort<T>(begin, mid);
      sort<T>(mid, end);

      return;
    }
#endif

  TRY_PARALLEL_2(std::sort, begin, end);
}

template <class T>
void sort(T *begin, T *end, std::function<int(T, T)> compare)
{
#ifndef __clang__
  size_t size_elements = (end - begin);
  size_t size_bytes = size_elements * sizeof(T);
  if((size_bytes >= SORT_MAX_PARALLEL_BYTES) && (size_elements >= 2))
    {
      // divide and conquer so extra working space doesn't exhaust
      // RAM. this has happened several times while sorting memory
      // mapped files close to 2x RAM.
      T *mid = begin + size_elements / 2;

      // parallel policy is allowed O(n log n) swaps, and uncertain
      // about working space, so stay serial.
      std::nth_element(begin, mid, end, compare);

      sort<T>(begin, mid, compare);
      sort<T>(mid, end, compare);

      return;
    }
#endif

  TRY_PARALLEL_3(std::sort, begin, end, compare);
}

template <class T>
T *sort_unique(T *begin, T *end)
{
  T *dest = begin;

  std::vector<T *> ends = {end};
  while(ends.size())
    {
      end = ends.back();
      ends.pop_back();
      assert(begin <= end);

      if(begin == end)
	{
	  // empty partition
	  continue;
	}

      if((end - begin) * sizeof(T) <= 1ULL << 20)
	{
	  // base case : less than 1MB
	  TRY_PARALLEL_2(std::sort, begin, end);
	  end = TRY_PARALLEL_2(std::unique, begin, end);

	  // copy carefully
	  if((begin - dest) >= (end - begin))
	    {
	      // no overlap -> parallel copy
	      dest = TRY_PARALLEL_3(std::copy, begin, end, dest);
	    }
	  else
	    {
	      // overlap -> serial copy
	      dest = std::copy(begin, end, dest);
	    }

	  begin = end;
	  continue;
	}

      // get range of values in current range

      T value_min = *TRY_PARALLEL_2(std::min_element, begin, end);
      T value_max = *TRY_PARALLEL_2(std::max_element, begin, end);
      if(value_min == value_max)
	{
	  // only one value in this range
	  *dest++ = value_min;
	  begin = end;
	  continue;
	}

      // partition values into 32 buckets

      const T target_buckets = 32;
      T divisor = (value_max - value_min + (target_buckets - 2)) / (target_buckets - 1);

      std::vector<T *> partition = flashsort_partition<T, T>(begin, end, [=](const T& v){return (v - value_min) / divisor;});
      assert(partition.at(0) == begin);
      assert(partition.back() == end);
      if(partition.size() > target_buckets + 1)
	{
	  std::cout << "value range = [" << value_min << "," << value_max << "], divisor = " << divisor << std::endl;
	}
      assert(partition.size() <= 33);
      for(size_t i = partition.size() - 1; i > 0; --i)
	{
	  ends.push_back(partition[i]);
	}
    }

  return dest;
}

// template instantiations

#include "DFA.h"

template void sort<dfa_state_t>(dfa_state_t *, dfa_state_t *, std::function<int(dfa_state_t, dfa_state_t)>);
template void sort<size_t>(size_t *, size_t *);

template size_t *sort_unique<size_t>(size_t *, size_t *);
