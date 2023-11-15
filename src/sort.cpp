// sort.cpp

// sort wrappers to go parallel when possible, but limit working set
// size since std::sort may blow out RAM.

#include "sort.h"

#include <algorithm>
#include <cassert>
#include <execution>

#ifdef __cpp_lib_parallel_algorithm
#define TRY_PARALLEL_2(f, a, b) f(std::execution::par_unseq, a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(std::execution::par_unseq, a, b, c)
#else
#define TRY_PARALLEL_2(f, a, b) f(a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(a, b, c)
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
#ifdef __cpp_lib_parallel_algorithm
  size_t size_elements = (end - begin);
  size_t size_bytes = size_elements * sizeof(T);
  if((size_bytes >= SORT_MAX_PARALLEL_BYTES) && (size_elements >= 8))
    {
      T *mid = begin + size_elements / 2;

      // parallel policy is allowed O(n log n) swaps, and uncertain
      // about working space, so stay serial.
      std::nth_element(begin, mid, end);

      // first range: [begin, mid) -> [begin, first_end)
      T *first_end = sort_unique<T>(begin, mid);
      assert(begin < first_end);
      if(*(first_end - 1) == *mid)
	{
	  // remove overlap with [mid, end)
	  --first_end;
	}
      ssize_t first_space = first_end - begin;

      T *mid_2 = mid + size_elements / 4;
      assert(mid < mid_2);
      assert(mid_2 < end);

      // second range: [mid, mid_2) -> [mid, second_end)
      T *second_end = sort_unique<T>(mid, mid_2);
      ssize_t second_size = second_end - mid;

      // third range: [mid_2, end) -> [mid_2, third_end)
      T *third_end = sort_unique<T>(mid_2, end);
      ssize_t third_size = third_end - mid_2;

      // merge second and third range behind first if space

      if(second_size + third_size <= first_space)
	{
	  return std::set_union(std::execution::par_unseq,
				mid, second_end,
				mid_2, third_end,
				first_end);
	}

      // move third range behind second and merge them

      T *third_copy_end = std::copy(std::execution::par_unseq,
				    mid_2, third_end,
				    second_end);
      assert(third_copy_end == second_end + third_size);

      std::inplace_merge(mid, second_end, third_copy_end);

      T *merged_unique_end = std::unique(mid, third_copy_end);

      // move combined second and third ranges behind first range

      return std::copy(std::execution::par_unseq,
		       mid, merged_unique_end,
		       first_end);
    }

  if(size_elements >= 1048576) // simple parallel
    {
      end = std::unique(std::execution::par_unseq, begin, end);
      std::sort(std::execution::par_unseq, begin, end);
      return std::unique(std::execution::par_unseq, begin, end);
    }
#endif

  // base case
  end = std::unique(begin, end);
  std::sort(begin, end);
  return std::unique(begin, end);
}

// template instantiations

#include "DFA.h"

template void sort<dfa_state_t>(dfa_state_t *, dfa_state_t *, std::function<int(dfa_state_t, dfa_state_t)>);
template void sort<size_t>(size_t *, size_t *);

template size_t *sort_unique<size_t>(size_t *, size_t *);
