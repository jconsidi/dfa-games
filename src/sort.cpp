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
  T *begin_original = begin;
  T *dest = begin;

  auto handle_overlap = [&](T *next)
  {
    // if [begin_original, dest) ends with value at next, pop it off
    // to avoid duplicate when [next, ...) is copied.
    if((dest > begin_original) && (*(dest-1) == *next))
      {
	--dest;
      }
  };

  while(begin + (1ULL << 20) < end)
    {
      // large remaining unsorted region

      assert(dest <= begin);

      if(begin - dest >= end - begin)
	{
	  // [dest, begin) has room for the remainder without further
	  // shrinkage.
	  //
	  // process two halves independently, then merge at
	  // destination.

	  T *mid = begin + (end - begin) / 2;
	  assert(begin < mid);
	  assert(mid < end);

	  // [begin, mid)
	  T *begin_end = sort_unique(begin, mid);
	  handle_overlap(begin);

	  // [mid, end)
	  T *mid_end = sort_unique(mid, end);
	  handle_overlap(mid);

	  // output
	  dest = TRY_PARALLEL_5(std::set_union, begin, begin_end, mid, mid_end, dest);
	  assert(dest <= begin);

	  begin = end;
	}
      else if(begin - dest >= (end - begin) / 16)
	{
	  // [dest, begin) can take a minimum fraction of the
	  // remainder.
	  //
	  // split off a chunk that will fit before unique.

	  T *mid = begin + (begin - dest);
	  assert(begin < mid);
	  assert(mid < end);
	  assert(mid - begin <= begin - dest);
	  std::nth_element(begin, mid, end);

	  // [begin, mid)
	  T *begin_end = sort_unique(begin, mid);
	  assert(begin_end - begin <= begin - dest);
	  handle_overlap(begin);

	  // output
	  dest = TRY_PARALLEL_3(std::copy, begin, begin_end, dest);

	  begin = mid;
	}
      else
	{
	  // not much room beween dest and begin so handle a small chunk
	  // and copy handling overlap.
	  T *mid = begin + (end - begin) / 16;
	  assert(begin < mid);
	  assert(mid < end);
	  std::nth_element(begin, mid, end);

	  // [begin, mid)
	  T *begin_end = sort_unique(begin, mid);
	  handle_overlap(begin);

	  // output - serial copy because of overlap
	  dest = std::copy(begin, begin_end, dest);

	  begin = mid;
	}
    }

  // base case

  if(begin < end)
    {
      TRY_PARALLEL_2(std::sort, begin, end);
      end = TRY_PARALLEL_2(std::unique, begin, end);
      handle_overlap(begin);

      if(dest < begin)
	{
	  // need to move [begin, end) to [dest,...)

	  if(dest + (end - begin) < begin)
	    {
	      // ranges do not overlap
	      dest = TRY_PARALLEL_3(std::copy, begin, end, dest);
	    }
	  else
	    {
	      // must copy serially because of overlapping range.
	      dest = std::copy(begin, end, dest);
	    }
	}
      else
	{
	  dest = end;
	}

      begin = end;
    }
  assert(begin == end);

  return dest;
}

// template instantiations

#include "DFA.h"

template void sort<dfa_state_t>(dfa_state_t *, dfa_state_t *, std::function<int(dfa_state_t, dfa_state_t)>);
template void sort<size_t>(size_t *, size_t *);

template size_t *sort_unique<size_t>(size_t *, size_t *);
