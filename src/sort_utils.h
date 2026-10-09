// sort_utils.h

#ifndef SORT_UTILS_H
#define SORT_UTILS_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <unistd.h>
#include <vector>

#include "Profile.h"
#include "parallel.h"

// Parallel sort that needs no buffer the size of its input.
//
// libstdc++ implements std::sort(par) as a parallel merge sort with a
// buffer as large as the input. For inputs near the size of RAM (the
// BinaryDFA forward and backward sorts reach tens of GB) that buffer pushes
// the sort into swap. std::partition(par) is in place, so this is a
// quicksort built from it:
//
// 1. While a range is longer than split_threshold, partition it three ways
//    around a sampled pivot with two parallel std::partition calls. The
//    range equal to the pivot is final. It is never empty, since the pivot
//    is an element of the range, so every split makes progress, and it keeps
//    many duplicate keys from degrading the split.
// 2. Sort the remaining ranges in parallel, each with serial std::sort.
// 3. Check the whole input with a parallel std::is_sorted, and throw if it
//    is not sorted, so the result is checked rather than trusted.
//
// The sort is not stable. Extra memory is the list of ranges.
//
// split_threshold 0 picks a default from the input size and thread count.
// Tests pass small values to run the split path on small inputs.

inline size_t parallel_sort_default_split_threshold(size_t n)
{
  size_t threads = std::max(size_t(std::thread::hardware_concurrency()), size_t(1));
  return std::max(n / (8 * threads), size_t(1) << 16);
}

template <class T>
T parallel_sort_sample_pivot(const T *begin, const T *end)
{
  assert(begin < end);

  const size_t samples_max = 31;
  size_t n = size_t(end - begin);
  size_t samples_count = std::min(n, samples_max);

  // Random positions rather than a fixed stride, which periodic structure in
  // the input could line up with to keep choosing bad pivots. Seeded from
  // the range size so a given input always gets the same pivots, keeping a
  // slow or failing sort reproducible. Bad pivots only cost time: every
  // split still removes the elements equal to the pivot.
  std::mt19937_64 rng(0x9E3779B97F4A7C15ULL ^ uint64_t(n));
  std::uniform_int_distribution<size_t> position(0, n - 1);

  std::vector<T> samples;
  samples.reserve(samples_count);
  for(size_t s = 0; s < samples_count; ++s)
    {
      samples.push_back(begin[position(rng)]);
    }

  auto middle = samples.begin() + std::ptrdiff_t(samples_count / 2);
  std::nth_element(samples.begin(), middle, samples.end());
  return *middle;
}

template <class T>
void parallel_sort_in_place(T *begin, T *end, size_t split_threshold = 0)
{
  assert(begin <= end);
  size_t n = size_t(end - begin);

  if(split_threshold == 0)
    {
      split_threshold = parallel_sort_default_split_threshold(n);
    }

  typedef std::pair<T *, T *> range_t;
  std::vector<range_t> pending;
  std::vector<range_t> leaves;
  pending.emplace_back(begin, end);

  while(pending.size() > 0)
    {
      range_t range = pending.back();
      pending.pop_back();

      if(size_t(range.second - range.first) <= split_threshold)
        {
          if(range.first < range.second)
            {
              leaves.push_back(range);
            }
          continue;
        }

      T pivot = parallel_sort_sample_pivot<T>(range.first, range.second);

      auto less_than_pivot = [&pivot](const T& v)
      {
        return v < pivot;
      };
      T *equal_begin = TRY_PARALLEL_3(std::partition, range.first, range.second, less_than_pivot);

      auto not_greater_than_pivot = [&pivot](const T& v)
      {
        return !(pivot < v);
      };
      T *equal_end = TRY_PARALLEL_3(std::partition, equal_begin, range.second, not_greater_than_pivot);

      // pivot came from the range, so at least one element equals it
      if(!(equal_begin < equal_end))
        {
          throw std::logic_error("parallel_sort_in_place: partition around a sampled pivot found no element equal to it");
        }

      pending.emplace_back(range.first, equal_begin);
      pending.emplace_back(equal_end, range.second);
    }

  auto sort_leaf = [](const range_t& leaf)
  {
    std::sort(leaf.first, leaf.second);
  };
  TRY_PARALLEL_3(std::for_each, leaves.begin(), leaves.end(), sort_leaf);

  if(!TRY_PARALLEL_2(std::is_sorted, begin, end))
    {
      throw std::logic_error("parallel_sort_in_place: result of " + std::to_string(n) + " elements is not sorted");
    }
}

// Physical memory in bytes, from sysconf, which both macOS and Linux
// provide.
inline size_t physical_memory_bytes()
{
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  if((pages <= 0) || (page_size <= 0))
    {
      throw std::runtime_error("physical_memory_bytes: sysconf failed");
    }

  return size_t(pages) * size_t(page_size);
}

// std::sort(par) is about 2.4 times faster than parallel_sort_in_place when
// everything fits in RAM (2e9 uint64_t on a 128 GB machine), but needs a
// buffer as large as its input. Use it only while input plus buffer fit in
// this fraction of physical memory. The callers' inputs are mmapped files,
// and the DFAs being combined are mapped at the same time, so the fraction
// leaves them most of RAM. Tests set it to SIZE_MAX to force the in-place
// path on small inputs.
inline size_t parallel_sort_buffered_memory_divisor = 4;

template <class T>
bool parallel_sort_needs_in_place(size_t n)
{
  return 2 * n * sizeof(T) > physical_memory_bytes() / parallel_sort_buffered_memory_divisor;
}

// Sort with std::sort(par) when its buffer fits comfortably, and in place
// otherwise. Either way the result is checked and a failure throws. The
// path taken is recorded as this function's Profile phase ("in place" or
// "buffered"), nested under the caller's phase.
template <class T>
void parallel_sort(T *begin, T *end)
{
  assert(begin <= end);
  size_t n = size_t(end - begin);

  Profile profile("parallel_sort");

  if(parallel_sort_needs_in_place<T>(n))
    {
      profile.tic("in place");
      parallel_sort_in_place(begin, end);
      return;
    }

  profile.tic("buffered");
  TRY_PARALLEL_2(std::sort, begin, end);

  profile.tic("buffered check");

  if(!TRY_PARALLEL_2(std::is_sorted, begin, end))
    {
      throw std::logic_error("parallel_sort: result of " + std::to_string(n) + " elements is not sorted");
    }
}

#endif
