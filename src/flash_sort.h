// flash_sort.h

#ifndef FLASH_SORT_H
#define FLASH_SORT_H

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "parallel.h"

// A pair's row of transitions and its rank, sorted by the row.
//
// Rows compare as their transitions read as numbers, transition 0 first,
// which is the order BinaryDFA's direct keys give today. As a key, a row is
// the bit string of its transitions concatenated most significant bit
// first, key_bits long.
template <class id_t, int shape, class rank_t>
struct RowRankRecord
{
  static_assert(shape > 0);

  typedef id_t id_type;
  typedef rank_t rank_type;
  static constexpr int row_shape = shape;

  static constexpr int id_bits = int(8 * sizeof(id_t));
  static constexpr int key_bits = shape * id_bits;

  id_t row[shape];
  rank_t rank;

  // The 8 key bits starting at bit b, zero padded past the end of the key.
  uint8_t key_byte_at_bit(int b) const
  {
    assert((0 <= b) && (b < key_bits));
    int t = b / id_bits;
    int available = id_bits - b % id_bits;
    uint64_t value = uint64_t(row[t]);
    if(available >= 8)
      {
        return uint8_t((value >> (available - 8)) & 0xFF);
      }

    uint64_t high = value & ((uint64_t(1) << available) - 1);
    uint64_t low = 0;
    if(t + 1 < shape)
      {
        low = uint64_t(row[t + 1]) >> (id_bits - (8 - available));
      }
    return uint8_t((high << (8 - available)) | low);
  }

  // First key bit at or after b where this row and other differ, or
  // key_bits if they agree from b on.
  int first_difference_bit(const RowRankRecord& other, int b) const
  {
    assert((0 <= b) && (b <= key_bits));
    for(int t = b / id_bits; t < shape; ++t)
      {
        uint64_t difference = uint64_t(row[t] ^ other.row[t]);
        if(t == b / id_bits)
          {
            // only the bits from b on
            uint64_t id_mask = (id_bits == 64) ? ~uint64_t(0) : ((uint64_t(1) << id_bits) - 1);
            difference &= id_mask >> (b % id_bits);
          }
        if(difference != 0)
          {
            return t * id_bits + (std::countl_zero(difference) - (64 - id_bits));
          }
      }
    return key_bits;
  }

  bool row_less(const RowRankRecord& b) const
  {
    return std::lexicographical_compare(row, row + shape, b.row, b.row + shape);
  }

  bool row_equal(const RowRankRecord& b) const
  {
    return std::equal(row, row + shape, b.row);
  }
};

// Most significant digit first, in place radix sort by row, 8 key bits per
// level (American flag sort, the byte at a time form of flashsort).
//
// A level splits a range into 256 buckets by the 8 key bits after those the
// whole range shares, then sorts the buckets depth first, so once a bucket
// fits in memory it stays resident until it is finished.
//
// Skipping the shared bits means the digit always splits on bits that vary:
// a skewed range (rows starting with a reject transition) costs one pass
// rather than one per shared digit, and ids that use only the low 33 of
// their 64 bits still split many ways at the top. The scan for them stops
// as soon as they run out, which for most ranges is within a few elements.
//
// A level splits a range one of two ways:
//
// - Serially (ranges under region_min elements): count the digits, then
//   cycle each displaced element to the next free slot of its bucket. Each
//   bucket fills from one advancing pointer, so the level touches 256
//   forward moving positions rather than random ones.
//
// - By regions (after RegionSort, Obeya et al., SPAA 2019): split the range
//   into region_chunks chunks and split each chunk serially, in parallel.
//   Each chunk is then a sequence of up to 256 runs, one per digit. Clipped
//   to the buckets, a run of digit d inside bucket b != d is a misplaced
//   piece. Pieces only need to reach their bucket, not a particular place
//   in it, so swaps choose pieces that place as much as possible: first
//   digit d inside b with digit b inside d, which places both sides, pairs
//   of buckets in parallel; then the one way flows left, by following
//   cycles. Every swap is one swap_ranges over contiguous memory, in
//   parallel when it is long, so every byte moves sequentially.
//
// Ranges of at most insertion_max elements finish with insertion sort.
// Ranges of more than parallel_min elements scan and count in parallel and
// sort their buckets in parallel. Ranges of at least verbose_min elements
// log their timings.
//
// Not stable: equal rows come out in any order. The result is checked with
// a parallel is_sorted, and a failure throws.
template <class T>
class FlashSort
{
  static constexpr int key_bits = T::key_bits;

  size_t insertion_max;
  size_t parallel_min;
  size_t verbose_min;
  size_t region_min;
  size_t region_chunks;

  typedef std::array<size_t, 256> counts_t;

  static void insertion_sort(T *begin, T *end)
  {
    for(T *i = begin + 1; i < end; ++i)
      {
        T value = *i;
        T *j = i;
        while((j > begin) && value.row_less(j[-1]))
          {
            *j = j[-1];
            --j;
          }
        *j = value;
      }
  }

  // Number of key bits from bit on that every element of [begin, end)
  // shares with first.
  static int shared_bits_serial(const T& first, const T *begin, const T *end, int bit, int shared_end)
  {
    for(const T *p = begin; (p < end) && (shared_end > bit); ++p)
      {
        shared_end = std::min(shared_end, p->first_difference_bit(first, bit));
      }
    return shared_end;
  }

  // Number of key bits from bit on that every element shares with the first.
  int shared_bits(const T *begin, const T *end, int bit) const
  {
    size_t n = size_t(end - begin);
    const T& first = *begin;

    if(n <= parallel_min)
      {
        return shared_bits_serial(first, begin + 1, end, bit, key_bits) - bit;
      }

    // Chunks in parallel, each stopping once any chunk has found the
    // shared bits run out at bit.
    std::atomic<int> shared_end(key_bits);
    size_t chunk_count = 256;
    size_t chunk_size = (n + chunk_count - 1) / chunk_count;
    std::vector<size_t> chunks(chunk_count);
    for(size_t c = 0; c < chunk_count; ++c)
      {
        chunks[c] = c;
      }
    auto scan_chunk = [&](size_t c)
    {
      const T *chunk_begin = begin + std::min(n, c * chunk_size);
      const T *chunk_end = begin + std::min(n, (c + 1) * chunk_size);
      const size_t step = 4096;
      for(const T *p = chunk_begin; p < chunk_end; p += std::min(step, size_t(chunk_end - p)))
        {
          int current = shared_end.load(std::memory_order_relaxed);
          if(current <= bit)
            {
              return;
            }
          int found = shared_bits_serial(first, p, std::min(p + step, chunk_end), bit, current);
          int expected = shared_end.load(std::memory_order_relaxed);
          while((found < expected) && !shared_end.compare_exchange_weak(expected, found, std::memory_order_relaxed))
            {
            }
        }
    };
    TRY_PARALLEL_3(std::for_each, chunks.begin(), chunks.end(), scan_chunk);

    return std::max(shared_end.load(), bit) - bit;
  }

  static counts_t count_digits_serial(const T *begin, const T *end, int bit)
  {
    counts_t counts{};
    for(const T *p = begin; p < end; ++p)
      {
        ++counts[p->key_byte_at_bit(bit)];
      }
    return counts;
  }

  counts_t count_digits(const T *begin, const T *end, int bit) const
  {
    size_t n = size_t(end - begin);
    if(n <= parallel_min)
      {
        return count_digits_serial(begin, end, bit);
      }

    size_t chunk_count = 256;
    size_t chunk_size = (n + chunk_count - 1) / chunk_count;
    std::vector<counts_t> chunk_counts(chunk_count);
    std::vector<size_t> chunks(chunk_count);
    for(size_t c = 0; c < chunk_count; ++c)
      {
        chunks[c] = c;
      }

    auto count_chunk = [&](size_t c)
    {
      chunk_counts[c] = count_digits_serial(begin + std::min(n, c * chunk_size), begin + std::min(n, (c + 1) * chunk_size), bit);
    };
    TRY_PARALLEL_3(std::for_each, chunks.begin(), chunks.end(), count_chunk);

    counts_t counts{};
    for(const auto& local : chunk_counts)
      {
        for(int b = 0; b < 256; ++b)
          {
            counts[b] += local[b];
          }
      }
    return counts;
  }

  // Ends of each digit's bucket, given its counts, from begin.
  static std::array<T *, 256> bucket_ends(T *begin, const counts_t& counts)
  {
    std::array<T *, 256> ends;
    T *position = begin;
    for(int b = 0; b < 256; ++b)
      {
        position += counts[b];
        ends[b] = position;
      }
    return ends;
  }

  // Serial split of [begin, end) into its digits' buckets, given their
  // counts. Returns each bucket's end.
  static std::array<T *, 256> permute_serial(T *begin, T *end, int bit, const counts_t& counts)
  {
    std::array<T *, 256> bucket_end = bucket_ends(begin, counts);
    if(bucket_end[255] != end)
      {
        throw std::logic_error("FlashSort: digit counts sum to " + std::to_string(bucket_end[255] - begin) + " of " + std::to_string(end - begin) + " elements");
      }

    std::array<T *, 256> next;
    next[0] = begin;
    for(int b = 1; b < 256; ++b)
      {
        next[b] = bucket_end[b - 1];
      }

    // Cycle each displaced element to the next free slot of its bucket.
    for(int b = 0; b < 256; ++b)
      {
        while(next[b] < bucket_end[b])
          {
            T value = *next[b];
            int digit = value.key_byte_at_bit(bit);
            while(digit != b)
              {
                assert(next[digit] < bucket_end[digit]);
                std::swap(value, *next[digit]);
                ++next[digit];
                digit = value.key_byte_at_bit(bit);
              }
            *next[b] = value;
            ++next[b];
          }
      }

    return bucket_end;
  }

  // Swaps of runs go parallel at a far smaller size than parallel_min,
  // since a top level run averages n / (region_chunks * 256) elements, and
  // the run loop moves nearly every byte of the range through here.
  static constexpr size_t parallel_swap_min = size_t(1) << 14;

  void swap_block(T *x, T *y, size_t k) const
  {
    if(k > std::min(parallel_min, parallel_swap_min))
      {
        TRY_PARALLEL_3(std::swap_ranges, x, x + k, y);
      }
    else
      {
        std::swap_ranges(x, x + k, y);
      }
  }

  // Region split of [begin, end) into its digits' buckets. Returns each
  // bucket's end.
  std::array<T *, 256> permute_regions(T *begin, T *end, int bit) const
  {
    size_t n = size_t(end - begin);
    size_t chunk_count = std::max(std::min(region_chunks, n), size_t(1));
    size_t chunk_size = (n + chunk_count - 1) / chunk_count;

    auto phase_start = std::chrono::steady_clock::now();

    // 1. Split each chunk serially, in parallel.
    std::vector<counts_t> chunk_counts(chunk_count);
    std::vector<size_t> chunks(chunk_count);
    for(size_t c = 0; c < chunk_count; ++c)
      {
        chunks[c] = c;
      }
    auto split_chunk = [&](size_t c)
    {
      T *chunk_begin = begin + std::min(n, c * chunk_size);
      T *chunk_end = begin + std::min(n, (c + 1) * chunk_size);
      chunk_counts[c] = count_digits_serial(chunk_begin, chunk_end, bit);
      permute_serial(chunk_begin, chunk_end, bit, chunk_counts[c]);
    };
    TRY_PARALLEL_3(std::for_each, chunks.begin(), chunks.end(), split_chunk);

    counts_t counts{};
    for(const auto& local : chunk_counts)
      {
        for(int b = 0; b < 256; ++b)
          {
            counts[b] += local[b];
          }
      }
    std::array<T *, 256> bucket_end = bucket_ends(begin, counts);
    if(bucket_end[255] != end)
      {
        throw std::logic_error("FlashSort: region digit counts sum to " + std::to_string(bucket_end[255] - begin) + " of " + std::to_string(n) + " elements");
      }

    double split_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - phase_start).count();
    phase_start = std::chrono::steady_clock::now();

    // 2. Pieces: each chunk run clipped to the buckets it overlaps. A piece
    //    of digit d inside bucket b != d is misplaced, and goes in
    //    misplaced[b][d] as (offset from begin, length). Lists only ever
    //    hold positions that currently hold their digit.
    struct piece_t
    {
      size_t start;
      size_t length;
    };
    std::vector<std::array<std::vector<piece_t>, 256>> misplaced(256);
    std::array<size_t, 257> bucket_start;
    bucket_start[0] = 0;
    for(int b = 0; b < 256; ++b)
      {
        bucket_start[b + 1] = size_t(bucket_end[b] - begin);
      }
    size_t initial_pieces = 0;
    for(size_t c = 0; c < chunk_count; ++c)
      {
        size_t position = std::min(n, c * chunk_size);
        for(int d = 0; d < 256; ++d)
          {
            size_t run_end = position + chunk_counts[c][d];
            while(position < run_end)
              {
                // bucket holding position
                int b = int(std::upper_bound(bucket_start.begin(), bucket_start.end(), position) - bucket_start.begin()) - 1;
                assert((0 <= b) && (b < 256));
                size_t piece_end = std::min(run_end, bucket_start[b + 1]);
                if(b != d)
                  {
                    misplaced[b][d].push_back({position, piece_end - position});
                    ++initial_pieces;
                  }
                position = piece_end;
              }
          }
      }

    size_t swaps = 0;
    size_t swapped = 0;

    // Swaps the first k elements of p and q, k the shorter length, trims
    // both from the front, and returns k.
    auto swap_pieces = [&](piece_t& p, piece_t& q)
    {
      size_t k = std::min(p.length, q.length);
      assert(k > 0);
      swap_block(begin + p.start, begin + q.start, k);
      p.start += k;
      p.length -= k;
      q.start += k;
      q.length -= k;
      return k;
    };

    // 3. Two-cycles: digit d inside bucket b swapped with digit b inside
    //    bucket d places both sides, and each swap empties a piece, so
    //    there are at most as many swaps as pieces. Each pair owns its two
    //    lists, so pairs run in parallel.
    std::vector<std::pair<int, int>> pairs;
    for(int b = 0; b < 256; ++b)
      {
        for(int d = b + 1; d < 256; ++d)
          {
            if((misplaced[b][d].size() > 0) && (misplaced[d][b].size() > 0))
              {
                pairs.emplace_back(b, d);
              }
          }
      }
    std::atomic<size_t> pair_swaps(0);
    std::atomic<size_t> pair_swapped(0);
    auto swap_pair = [&](const std::pair<int, int>& pair)
    {
      std::vector<piece_t>& from = misplaced[pair.first][pair.second];
      std::vector<piece_t>& to = misplaced[pair.second][pair.first];
      size_t local_swaps = 0;
      size_t local_swapped = 0;
      while((from.size() > 0) && (to.size() > 0))
        {
          local_swapped += swap_pieces(from.back(), to.back());
          ++local_swaps;
          if(from.back().length == 0)
            {
              from.pop_back();
            }
          if(to.back().length == 0)
            {
              to.pop_back();
            }
        }
      pair_swaps += local_swaps;
      pair_swapped += local_swapped;
    };
    TRY_PARALLEL_3(std::for_each, pairs.begin(), pairs.end(), swap_pair);
    swaps += pair_swaps;
    swapped += pair_swapped;
    size_t cycle_swaps_start = swaps;

    // 4. Longer cycles. Now no pair has pieces both ways. Follow each
    //    misplaced piece of bucket b: swapping it with a misplaced piece q
    //    of bucket d (holding some e != d) places the swapped part of q for
    //    good, and leaves digit e where p was, which is placed if e is b and
    //    otherwise a new misplaced piece of bucket b.
    for(int b = 0; b < 256; ++b)
      {
        for(int d = 0; d < 256; ++d)
          {
            if(misplaced[b][d].size() == 0)
              {
                continue;
              }
              {
                piece_t& p = misplaced[b][d].back();

                // Prefer a q at least as long as p, so p moves whole and
                // leaves exactly one piece behind instead of splitting, and
                // among those one of digit b, which closes the cycle.
                // Otherwise the longest q there is.
                int e = -1;
                size_t q_index = 0;
                for(int f = 0; f < 256; ++f)
                  {
                    const std::vector<piece_t>& candidates = misplaced[d][f];
                    for(size_t i = 0; i < candidates.size(); ++i)
                      {
                        bool better;
                        if(e < 0)
                          {
                            better = true;
                          }
                        else
                          {
                            const piece_t& best = misplaced[d][e][q_index];
                            bool covers = candidates[i].length >= p.length;
                            bool best_covers = best.length >= p.length;
                            if(covers != best_covers)
                              {
                                better = covers;
                              }
                            else if(covers && ((f == b) != (e == b)))
                              {
                                better = (f == b);
                              }
                            else
                              {
                                better = candidates[i].length > best.length;
                              }
                          }
                        if(better)
                          {
                            e = f;
                            q_index = i;
                          }
                      }
                  }
                if(e < 0)
                  {
                    throw std::logic_error("FlashSort: bucket " + std::to_string(d) + " has no misplaced piece while a digit " + std::to_string(d) + " piece is in bucket " + std::to_string(b));
                  }
                assert(e != d);
                std::swap(misplaced[d][e][q_index], misplaced[d][e].back());
                piece_t& q = misplaced[d][e].back();

                size_t p_start = p.start;
                size_t k = swap_pieces(p, q);
                ++swaps;
                swapped += k;
                if(q.length == 0)
                  {
                    misplaced[d][e].pop_back();
                  }
                if(p.length == 0)
                  {
                    misplaced[b][d].pop_back();
                  }
                if(e != b)
                  {
                    misplaced[b][e].push_back({p_start, k});
                  }

                // Revisit d, and step back to e if its new piece is behind.
                d = std::min(d, e) - 1;
              }
          }
      }

    if(n >= verbose_min)
      {
        double swap_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - phase_start).count();
        static std::mutex log_mutex;
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "FlashSort: regions of " << n << " elements in " << chunk_count << " chunks: split " << split_seconds << "s, " << initial_pieces << " misplaced pieces, " << cycle_swaps_start << " pair swaps and " << (swaps - cycle_swaps_start) << " cycle swaps of " << swapped << " elements " << swap_seconds << "s" << std::endl;
      }

    return bucket_end;
  }

  void sort_range(T *begin, T *end, int bit) const
  {
    size_t n = size_t(end - begin);
    if((n < 2) || (bit >= key_bits))
      {
        return;
      }

    if(n <= insertion_max)
      {
        insertion_sort(begin, end);
        return;
      }

    bool verbose = (n >= verbose_min);
    auto time_start = std::chrono::steady_clock::now();
    auto seconds = [](std::chrono::steady_clock::time_point& since)
    {
      auto now = std::chrono::steady_clock::now();
      double elapsed = std::chrono::duration<double>(now - since).count();
      since = now;
      return elapsed;
    };

    int shared = shared_bits(begin, end, bit);
    bit += shared;
    if(bit >= key_bits)
      {
        // every row in the range is equal
        return;
      }
    double shared_seconds = seconds(time_start);

    bool regions = (n >= region_min);
    std::array<T *, 256> bucket_end;
    if(regions)
      {
        bucket_end = permute_regions(begin, end, bit);
      }
    else
      {
        bucket_end = permute_serial(begin, end, bit, count_digits(begin, end, bit));
      }
    double permute_seconds = seconds(time_start);

    std::vector<std::pair<T *, T *>> buckets;
    size_t largest_bucket = 0;
    T *bucket_begin = begin;
    for(int b = 0; b < 256; ++b)
      {
        if(bucket_begin < bucket_end[b])
          {
            buckets.emplace_back(bucket_begin, bucket_end[b]);
            largest_bucket = std::max(largest_bucket, size_t(bucket_end[b] - bucket_begin));
          }
        bucket_begin = bucket_end[b];
      }

    if(verbose)
      {
        static std::mutex log_mutex;
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "FlashSort: " << n << " elements at bit " << bit << " (" << shared << " shared): shared " << shared_seconds << "s, " << (regions ? "regions " : "count and permute ") << permute_seconds << "s, " << buckets.size() << " buckets, largest " << largest_bucket << std::endl;
      }

    auto sort_bucket = [&](const std::pair<T *, T *>& bucket)
    {
      sort_range(bucket.first, bucket.second, bit + 8);
    };
    if(n > parallel_min)
      {
        TRY_PARALLEL_3(std::for_each, buckets.begin(), buckets.end(), sort_bucket);
      }
    else
      {
        std::for_each(buckets.begin(), buckets.end(), sort_bucket);
      }
  }

public:

  explicit FlashSort(size_t insertion_max_in = 32,
                     size_t parallel_min_in = size_t(1) << 20,
                     size_t verbose_min_in = SIZE_MAX,
                     size_t region_min_in = size_t(1) << 24,
                     size_t region_chunks_in = 0)
    : insertion_max(std::max(insertion_max_in, size_t(1))),
      parallel_min(parallel_min_in),
      verbose_min(verbose_min_in),
      region_min(region_min_in),
      region_chunks(region_chunks_in > 0 ? region_chunks_in : 4 * std::max(size_t(std::thread::hardware_concurrency()), size_t(1)))
  {
  }

  void sort(T *begin, T *end) const
  {
    assert(begin <= end);
    sort_range(begin, end, 0);

    auto less = [](const T& a, const T& b)
    {
      return a.row_less(b);
    };
    if(!TRY_PARALLEL_3(std::is_sorted, begin, end, less))
      {
        throw std::logic_error("FlashSort: result of " + std::to_string(end - begin) + " elements is not sorted");
      }
  }
};

template <class T>
void flash_sort(T *begin, T *end)
{
  FlashSort<T>().sort(begin, end);
}

#endif
