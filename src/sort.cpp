// sort.cpp

// sort wrappers to go parallel when possible, but limit working set
// size since std::sort may blow out RAM.

#include "sort.h"

#include <algorithm>
#include <execution>

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

  std::sort(std::execution::par_unseq, begin, end);
#else
  std::sort(begin, end);
#endif
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

  std::sort(std::execution::par_unseq, begin, end, compare);
#else
  std::sort(begin, end, compare);
#endif
}

// template instantiations

#include "DFA.h"

template void sort<dfa_state_t>(dfa_state_t *, dfa_state_t *, std::function<int(dfa_state_t, dfa_state_t)>);
template void sort<size_t>(size_t *, size_t *);
