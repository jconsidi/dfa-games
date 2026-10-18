// test_flash_sort.cpp

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <atomic>
#include <functional>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/sha.h>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

#include "BinaryDFA.h"
#include "MemoryMap.h"
#include "bucket_sort.h"
#include "flash_sort.h"
#include "sort_utils.h"

static const char *patterns[] = {"random", "small", "equal", "skewed", "narrow_bits", "sorted", "reverse", "prefix"};

template <class id_t, int shape, class rank_t>
std::vector<RowRankRecord<id_t, shape, rank_t>> make_records(const std::string& pattern, size_t n, uint64_t seed)
{
  typedef RowRankRecord<id_t, shape, rank_t> record_t;

  std::mt19937_64 rng(seed);
  std::vector<record_t> records(n);
  for(size_t i = 0; i < n; ++i)
    {
      record_t& record = records[i];
      record.rank = rank_t(i);
      for(int j = 0; j < shape; ++j)
        {
          uint64_t value;
          if(pattern == "random")
            {
              value = rng();
            }
          else if(pattern == "small")
            {
              value = rng() % 4;
            }
          else if(pattern == "equal")
            {
              value = 42;
            }
          else if(pattern == "skewed")
            {
              // most rows reject on their first transition
              value = ((j == 0) && (rng() % 10 != 0)) ? 0 : rng();
            }
          else if(pattern == "narrow_bits")
            {
              // ids of a next layer just over 2^32 states, or 2^20 for 4 byte ids
              value = rng() % ((sizeof(id_t) == 8) ? 4479239467ULL : (1ULL << 20));
            }
          else if(pattern == "sorted")
            {
              value = (j == shape - 1) ? i : 0;
            }
          else if(pattern == "reverse")
            {
              value = (j == shape - 1) ? n - i : 0;
            }
          else if(pattern == "prefix")
            {
              // every row shares all but its last transition
              value = (j == shape - 1) ? rng() % 1000 : 7;
            }
          else
            {
              throw std::logic_error("unknown pattern " + pattern);
            }
          record.row[j] = id_t(value);
        }
    }

  return records;
}

template <class id_t, int shape, class rank_t>
void test_case(const std::string& pattern, size_t n, size_t insertion_max, size_t parallel_min, size_t region_min, size_t region_chunks)
{
  typedef RowRankRecord<id_t, shape, rank_t> record_t;

  std::string name = pattern + " id_bytes=" + std::to_string(sizeof(id_t)) + " shape=" + std::to_string(shape) + " rank_bytes=" + std::to_string(sizeof(rank_t)) + " n=" + std::to_string(n) + " insertion_max=" + std::to_string(insertion_max) + " parallel_min=" + std::to_string(parallel_min) + " region_min=" + std::to_string(region_min) + " region_chunks=" + std::to_string(region_chunks);

  std::vector<record_t> input = make_records<id_t, shape, rank_t>(pattern, n, 0x5EED + n);

  std::vector<record_t> actual = input;
  FlashSort<record_t>(insertion_max, parallel_min, SIZE_MAX, region_min, region_chunks).sort(actual.data(), actual.data() + n);

  std::vector<record_t> expected = input;
  std::sort(expected.begin(), expected.end(), [](const record_t& a, const record_t& b) {return a.key_less(b);});

  std::vector<bool> seen(n, false);
  for(size_t i = 0; i < n; ++i)
    {
      if(!actual[i].row_equal(expected[i]))
        {
          throw std::logic_error(name + ": row " + std::to_string(i) + " differs from std::sort");
        }

      size_t rank = size_t(actual[i].rank);
      if((rank >= n) || seen[rank])
        {
          throw std::logic_error(name + ": rank " + std::to_string(rank) + " out of range or repeated");
        }
      seen[rank] = true;

      if(!actual[i].row_equal(input[rank]))
        {
          throw std::logic_error(name + ": rank " + std::to_string(rank) + " carries a row it did not start with");
        }
    }
}

template <class id_t, int shape, class rank_t>
size_t test_shape()
{
  size_t cases = 0;
  for(const char *pattern : patterns)
    {
      for(size_t n : {size_t(0), size_t(1), size_t(2), size_t(31), size_t(32), size_t(33), size_t(1000), size_t(100000)})
        {
          // default, radix all the way down, the parallel paths on small
          // inputs, and regions of 1, 3 and 7 chunks on small inputs
          test_case<id_t, shape, rank_t>(pattern, n, 32, size_t(1) << 20, size_t(1) << 24, 0);
          test_case<id_t, shape, rank_t>(pattern, n, 1, size_t(1) << 20, size_t(1) << 24, 0);
          test_case<id_t, shape, rank_t>(pattern, n, 1, 64, size_t(1) << 24, 0);
          test_case<id_t, shape, rank_t>(pattern, n, 1, 64, 64, 1);
          test_case<id_t, shape, rank_t>(pattern, n, 1, 64, 64, 3);
          test_case<id_t, shape, rank_t>(pattern, n, 32, size_t(1) << 20, 64, 7);
          cases += 6;
        }
    }
  return cases;
}

template <class S>
std::vector<dfa_state_pair<S>> make_pairs(const std::string& pattern, size_t n, uint64_t seed)
{
  std::mt19937_64 rng(seed);
  std::vector<dfa_state_pair<S>> pairs;
  pairs.reserve(n);
  const uint64_t s_max = std::numeric_limits<S>::max();
  for(size_t i = 0; i < n; ++i)
    {
      uint64_t left;
      uint64_t right;
      if(pattern == "random")
        {
          left = rng() & s_max;
          right = rng() & s_max;
        }
      else if(pattern == "small")
        {
          left = rng() % 4;
          right = rng() % 4;
        }
      else if(pattern == "equal")
        {
          left = 42;
          right = 7;
        }
      else if(pattern == "skewed")
        {
          // most pairs reject on the left
          left = (rng() % 10 != 0) ? 0 : (rng() & s_max);
          right = rng() & s_max;
        }
      else if(pattern == "narrow_bits")
        {
          // states of a layer just over 2^32 states, or 2^20 for 4 byte states
          uint64_t size = (sizeof(S) == 8) ? 4479239467ULL : (1ULL << 20);
          left = rng() % size;
          right = rng() % size;
        }
      else if(pattern == "sorted")
        {
          left = i >> 8;
          right = i & 0xFF;
        }
      else if(pattern == "reverse")
        {
          left = (n - i) >> 8;
          right = (n - i) & 0xFF;
        }
      else if(pattern == "prefix")
        {
          // every pair shares its left state
          left = 7;
          right = rng() % 1000;
        }
      else
        {
          throw std::logic_error("unknown pattern " + pattern);
        }
      pairs.emplace_back(left, right);
    }

  // the forward pass's sentinel pair, which must sort last
  if(n > 0)
    {
      for(size_t i = 0; i < n; i += 97)
        {
          pairs[i] = dfa_state_pair<S>(s_max, s_max);
        }
    }

  return pairs;
}

template <class S>
size_t test_pairs()
{
  typedef dfa_state_pair<S> pair_t;

  size_t cases = 0;
  for(const char *pattern : patterns)
    {
      for(size_t n : {size_t(0), size_t(1), size_t(2), size_t(31), size_t(32), size_t(33), size_t(1000), size_t(100000)})
        {
          std::vector<pair_t> input = make_pairs<S>(pattern, n, 0xFA1B + n);
          std::vector<pair_t> expected = input;
          std::sort(expected.begin(), expected.end());

          // the same configurations as test_shape
          const size_t configs[][4] = {{32, size_t(1) << 20, size_t(1) << 24, 0},
                                       {1, size_t(1) << 20, size_t(1) << 24, 0},
                                       {1, 64, size_t(1) << 24, 0},
                                       {1, 64, 64, 1},
                                       {1, 64, 64, 3},
                                       {32, size_t(1) << 20, 64, 7}};
          for(const auto& config : configs)
            {
              std::vector<pair_t> actual = input;
              FlashSort<pair_t>(config[0], config[1], SIZE_MAX, config[2], config[3]).sort(actual.data(), actual.data() + n);
              if(actual != expected)
                {
                  throw std::logic_error(std::string(pattern) + " state_bytes=" + std::to_string(sizeof(S)) + " n=" + std::to_string(n) + " insertion_max=" + std::to_string(config[0]) + " parallel_min=" + std::to_string(config[1]) + " region_min=" + std::to_string(config[2]) + " region_chunks=" + std::to_string(config[3]) + ": pairs differ from std::sort");
                }
              ++cases;
            }
        }
    }
  return cases;
}

// bucket_sort on files, with the budget lowered so the out of core path
// runs on small inputs.

static std::string bucket_test_dir;

// Bucket files left in the test directory.
static size_t bucket_files_left()
{
  size_t left = 0;
  for(const auto& entry : std::filesystem::directory_iterator(bucket_test_dir))
    {
      if(entry.path().filename().string().find(".bucket.") != std::string::npos)
        {
          ++left;
        }
    }
  return left;
}

// Orders records by key, then by their bytes, so two sorts that differ
// only in the order of equal keys compare equal afterwards.
template <class T>
static void sort_total(std::vector<T>& records)
{
  std::sort(records.begin(), records.end(), [](const T& a, const T& b)
  {
    if(a.key_less(b))
      {
        return true;
      }
    if(b.key_less(a))
      {
        return false;
      }
    return std::memcmp(&a, &b, sizeof(T)) < 0;
  });
}

// Sorts input through a file with bucket_sort, and checks the result is
// sorted by key and holds exactly the input's records, and that the path
// taken and the bucket files left are as expected.
template <class T>
static BucketSortStats check_bucket_sort(const std::string& name, const std::vector<T>& input, int digit_bit, std::optional<T> sentinel, bool expect_out_of_core)
{
  std::string path = bucket_test_dir + "/records";
  MemoryMap<T> map(path, input);
  BucketSortStats stats = bucket_sort(map, digit_bit, sentinel);

  if(stats.out_of_core != expect_out_of_core)
    {
      throw std::logic_error(name + ": out of core path " + (stats.out_of_core ? "taken" : "not taken"));
    }
  if(map.size() != input.size())
    {
      throw std::logic_error(name + ": " + std::to_string(map.size()) + " records after the sort, expected " + std::to_string(input.size()));
    }
  // an empty map is never mapped
  std::vector<T> output;
  if(map.size() > 0)
    {
      output.assign(map.begin(), map.end());
    }
  for(size_t i = 1; i < output.size(); ++i)
    {
      if(output[i].key_less(output[i - 1]))
        {
          throw std::logic_error(name + ": record " + std::to_string(i) + " is less than the one before it");
        }
    }
  std::vector<T> expected = input;
  sort_total(expected);
  sort_total(output);
  if((expected.size() > 0) && (std::memcmp(expected.data(), output.data(), expected.size() * sizeof(T)) != 0))
    {
      throw std::logic_error(name + ": sorted records are not the input's records");
    }
  if(bucket_files_left() != 0)
    {
      throw std::logic_error(name + ": " + std::to_string(bucket_files_left()) + " bucket files left");
    }
  map.unlink();
  return stats;
}

// bucket_populate from input: in index order at or under the cutoff, and
// sorted by key above it, with exactly the input's records either way,
// after_populate called once after the last record was made, and no bucket
// files left.
template <class T>
static BucketSortStats check_bucket_populate(const std::string& name, const std::vector<T>& input, int digit_bit, std::optional<T> sentinel, bool expect_out_of_core)
{
  std::string path = bucket_test_dir + "/populated";
  std::atomic<size_t> made(0);
  size_t calls = 0;
  size_t made_at_call = 0;
  std::function<T(size_t)> populate = [&](size_t i)
  {
    ++made;
    return input[i];
  };
  PopulatedRecords<T> populated = bucket_populate<T>(path, input.size(), populate, digit_bit, sentinel, [&]()
  {
    ++calls;
    made_at_call = made;
  });

  if(calls != 1)
    {
      throw std::logic_error(name + ": after_populate called " + std::to_string(calls) + " times");
    }
  if(made_at_call != input.size())
    {
      throw std::logic_error(name + ": after_populate called after " + std::to_string(made_at_call) + " of " + std::to_string(input.size()) + " records were made");
    }
  if(populated.sorted != expect_out_of_core)
    {
      throw std::logic_error(name + ": records " + (populated.sorted ? "sorted" : "not sorted"));
    }
  if(populated.map.size() != input.size())
    {
      throw std::logic_error(name + ": " + std::to_string(populated.map.size()) + " records, expected " + std::to_string(input.size()));
    }

  std::vector<T> output;
  if(populated.map.size() > 0)
    {
      output.assign(populated.map.begin(), populated.map.end());
    }
  if(!populated.sorted)
    {
      if((input.size() > 0) && (std::memcmp(input.data(), output.data(), input.size() * sizeof(T)) != 0))
        {
          throw std::logic_error(name + ": records are not in index order");
        }
    }
  else
    {
      for(size_t i = 1; i < output.size(); ++i)
        {
          if(output[i].key_less(output[i - 1]))
            {
              throw std::logic_error(name + ": record " + std::to_string(i) + " is less than the one before it");
            }
        }
      std::vector<T> expected = input;
      sort_total(expected);
      sort_total(output);
      if(std::memcmp(expected.data(), output.data(), expected.size() * sizeof(T)) != 0)
        {
          throw std::logic_error(name + ": sorted records are not the input's records");
        }
    }
  if(bucket_files_left() != 0)
    {
      throw std::logic_error(name + ": " + std::to_string(bucket_files_left()) + " bucket files left");
    }
  populated.map.unlink();
  return populated.stats;
}

// Records with every key bit before digit_bit clear: leading field values
// below bound, the rest of the key unconstrained. pattern random, equal
// (one record repeated), few (8 distinct), or skewed (90% share a leading
// value of 0, so one bucket is far over the budget).
template <class T, class Make>
static std::vector<T> make_bucket_records(const std::string& pattern, size_t n, uint64_t bound, Make make)
{
  std::mt19937_64 rng(n * 31 + pattern.size());
  std::vector<T> records;
  records.reserve(n);
  for(size_t i = 0; i < n; ++i)
    {
      uint64_t lead = rng() % bound;
      uint64_t rest = rng();
      if(pattern == "equal")
        {
          lead = bound / 2;
          rest = 12345;
        }
      else if(pattern == "few")
        {
          lead = (rest % 8) * (bound / 8);
          rest %= 8;
        }
      else if((pattern == "skewed") && (rest % 10 != 0))
        {
          lead = 0;
        }
      records.push_back(make(lead, rest, i));
    }
  return records;
}

template <class T, class Make>
static size_t test_bucket_sort_type(const std::string& type_name, uint64_t bound, int field_bits, Make make)
{
  size_t cases = 0;
  int digit_bit = bucket_digit_bit(field_bits, bound);

  for(std::string pattern : {"random", "equal", "few", "skewed"})
    {
      for(size_t n : {size_t(0), size_t(1), size_t(1000), size_t(50000)})
        {
          std::string name = "bucket_sort " + type_name + " " + pattern + " n=" + std::to_string(n);
          std::vector<T> input = make_bucket_records<T>(pattern, n, bound, make);

          // in memory at the production cutoff
          check_bucket_sort<T>(name + " in memory", input, digit_bit, std::nullopt, false);

          bool out_of_core = n * sizeof(T) > 4096;

          // bucket files whenever there is anything to sort, with the
          // production in-core limit, so few buckets and none too big
          bucket_sort_min_bytes = 4096;
          BucketSortStats wide = check_bucket_sort<T>(name + " buckets", input, digit_bit, std::nullopt, out_of_core);
          if(wide.repartitioned + wide.equal_runs != 0)
            {
              throw std::logic_error(name + " buckets: a bucket went over the production in-core limit");
            }

          // and with a 4KB in-core limit, so buckets over it are
          // partitioned again or, with all keys equal, passed on in pieces
          bucket_sort_memory_max = 4096;
          BucketSortStats narrow = check_bucket_sort<T>(name + " buckets, small limit", input, digit_bit, std::nullopt, out_of_core);
          BucketSortStats populated_narrow = check_bucket_populate<T>(name + " populate, small limit", input, digit_bit, std::nullopt, out_of_core);
          bucket_sort_memory_max = 0;
          check_bucket_populate<T>(name + " populate", input, digit_bit, std::nullopt, out_of_core);
          bucket_sort_min_bytes = 0;
          check_bucket_populate<T>(name + " populate in memory", input, digit_bit, std::nullopt, false);
          if((n == 50000) && (pattern == "skewed") && (populated_narrow.repartitioned == 0))
            {
              throw std::logic_error(name + " populate, small limit: no bucket was partitioned again");
            }
          if((n == 50000) && (pattern == "skewed") && (narrow.repartitioned == 0))
            {
              throw std::logic_error(name + " buckets, small limit: no bucket was partitioned again");
            }
          if((n == 50000) && (pattern == "equal") && (narrow.equal_runs == 0))
            {
              throw std::logic_error(name + " buckets, small limit: no bucket of equal keys was passed on in pieces");
            }
          cases += 6;
        }
    }

  // a record with a key bit set before digit_bit throws, leaving no bucket
  // files
  if(digit_bit > 0)
    {
      std::vector<T> input = make_bucket_records<T>("random", 50000, bound, make);
      input[31337] = make(bound | (uint64_t(1) << (field_bits - 1)), 0, 0);
      std::string path = bucket_test_dir + "/records";
      MemoryMap<T> map(path, input);
      bucket_sort_min_bytes = 4096;
      bucket_sort_memory_max = 4096;
      bool threw = false;
      try
        {
          bucket_sort(map, digit_bit);
        }
      catch(const std::logic_error& e)
        {
          threw = true;
          std::cout << "bucket_sort " << type_name << " bad record: threw as expected: " << e.what() << std::endl;
        }
      bucket_sort_memory_max = 0;
      bucket_sort_min_bytes = 0;
      if(!threw)
        {
          throw std::logic_error("bucket_sort " + type_name + ": a record with a bit set before digit_bit did not throw");
        }
      if(bucket_files_left() != 0)
        {
          throw std::logic_error("bucket_sort " + type_name + " bad record: " + std::to_string(bucket_files_left()) + " bucket files left");
        }
      ::unlink(path.c_str());
      ++cases;

      // the same record from a populate function
      bucket_sort_min_bytes = 4096;
      bucket_sort_memory_max = 4096;
      threw = false;
      try
        {
          std::function<T(size_t)> populate = [&](size_t i) {return input[i];};
          bucket_populate<T>(bucket_test_dir + "/populated", input.size(), populate, digit_bit, std::nullopt, []() {});
        }
      catch(const std::logic_error& e)
        {
          threw = true;
          std::cout << "bucket_populate " << type_name << " bad record: threw as expected: " << e.what() << std::endl;
        }
      bucket_sort_memory_max = 0;
      bucket_sort_min_bytes = 0;
      if(!threw)
        {
          throw std::logic_error("bucket_populate " + type_name + ": a record with a bit set before digit_bit did not throw");
        }
      if(bucket_files_left() != 0)
        {
          throw std::logic_error("bucket_populate " + type_name + " bad record: " + std::to_string(bucket_files_left()) + " bucket files left");
        }
      ++cases;
    }

  return cases;
}

// Pairs with sentinels (none, some, all), which sort last.
template <class S>
static size_t test_bucket_sort_sentinels()
{
  typedef dfa_state_pair<S> pair_t;
  const uint64_t bound = 3000000;
  const pair_t sentinel(std::numeric_limits<S>::max(), std::numeric_limits<S>::max());
  int digit_bit = bucket_digit_bit(int(8 * sizeof(S)), bound);

  size_t cases = 0;
  for(size_t every : {size_t(0), size_t(7), size_t(1)})
    {
      std::mt19937_64 rng(every);
      std::vector<pair_t> input;
      for(size_t i = 0; i < 50000; ++i)
        {
          bool is_sentinel = (every > 0) && (i % every == 0);
          input.push_back(is_sentinel ? sentinel : pair_t(rng() % bound, rng() % bound));
        }
      std::string name = "bucket_sort sentinels " + std::to_string(8 * sizeof(S)) + " bit every " + std::to_string(every);
      bucket_sort_min_bytes = 4096;
      check_bucket_sort<pair_t>(name, input, digit_bit, sentinel, true);
      check_bucket_populate<pair_t>(name + " populate", input, digit_bit, sentinel, true);
      bucket_sort_memory_max = 4096;
      check_bucket_sort<pair_t>(name + ", small limit", input, digit_bit, sentinel, true);
      check_bucket_populate<pair_t>(name + " populate, small limit", input, digit_bit, sentinel, true);
      bucket_sort_memory_max = 0;
      bucket_sort_min_bytes = 0;
      cases += 3;
    }
  return cases;
}

static void run_bucket_sort_tests()
{
  bucket_test_dir = (std::filesystem::temp_directory_path() / ("test_flash_sort." + std::to_string(getpid()))).string();
  std::filesystem::create_directories(bucket_test_dir);

  size_t cases = 0;
  cases += test_bucket_sort_type<dfa_state_pair<uint32_t>>("pairs 32", 3000000000ULL, 32, [](uint64_t lead, uint64_t rest, size_t)
  {
    return dfa_state_pair<uint32_t>(uint32_t(lead), uint32_t(rest));
  });
  cases += test_bucket_sort_type<dfa_state_pair<uint64_t>>("pairs 64", 4479239465ULL, 64, [](uint64_t lead, uint64_t rest, size_t)
  {
    return dfa_state_pair<uint64_t>(lead, rest % 4479239465ULL);
  });
  typedef RowRankRecord<uint32_t, 2, uint32_t> rows_32_t;
  cases += test_bucket_sort_type<rows_32_t>("rows 32 shape 2", 1000000, 32, [](uint64_t lead, uint64_t rest, size_t i)
  {
    rows_32_t record;
    record.row[0] = uint32_t(lead);
    record.row[1] = uint32_t(rest % 1000000);
    record.rank = uint32_t(i);
    return record;
  });
  typedef RowRankRecord<uint64_t, 4, uint64_t> rows_64_t;
  cases += test_bucket_sort_type<rows_64_t>("rows 64 shape 4", 9000000000ULL, 64, [](uint64_t lead, uint64_t rest, size_t i)
  {
    rows_64_t record;
    record.row[0] = lead;
    for(int j = 1; j < 4; ++j)
      {
        record.row[j] = (rest >> (8 * j)) % 9000000000ULL;
      }
    record.rank = i;
    return record;
  });
  typedef RankOutputRecord<uint64_t, uint64_t> ranks_t;
  cases += test_bucket_sort_type<ranks_t>("rank outputs", 50000, 64, [](uint64_t lead, uint64_t rest, size_t)
  {
    ranks_t record;
    record.row[0] = lead;
    record.rank = rest;
    return record;
  });
  // a bound of 1, so every key is zero and digit_bit is the whole key
  {
    std::vector<ranks_t> input(5000);
    for(size_t i = 0; i < input.size(); ++i)
      {
        input[i].row[0] = 0;
        input[i].rank = i;
      }
    bucket_sort_min_bytes = 4096;
    check_bucket_sort<ranks_t>("bucket_sort rank outputs bound 1", input, bucket_digit_bit(64, 1), std::nullopt, true);
    bucket_sort_min_bytes = 0;
    ++cases;
  }
  cases += test_bucket_sort_sentinels<uint32_t>();
  cases += test_bucket_sort_sentinels<uint64_t>();

  std::filesystem::remove_all(bucket_test_dir);
  std::cout << "test_flash_sort: " << cases << " bucket_sort and bucket_populate cases sorted by key (or left in index order in memory) with exactly their input records and no bucket files left, or threw" << std::endl;
}

static void run_tests()
{
  size_t cases = 0;
  cases += test_shape<uint32_t, 1, uint64_t>();
  cases += test_shape<uint32_t, 2, uint64_t>();
  cases += test_shape<uint32_t, 3, uint32_t>();
  cases += test_shape<uint32_t, 5, uint64_t>();
  cases += test_shape<uint32_t, 13, uint64_t>();
  cases += test_shape<uint64_t, 1, uint64_t>();
  cases += test_shape<uint64_t, 2, uint64_t>();
  cases += test_shape<uint64_t, 3, uint64_t>();
  cases += test_shape<uint64_t, 13, uint64_t>();

  std::cout << "test_flash_sort: " << cases << " cases matched std::sort by row, with every rank present once and carrying its own row" << std::endl;

  size_t pair_cases = 0;
  pair_cases += test_pairs<uint32_t>();
  pair_cases += test_pairs<uint64_t>();

  std::cout << "test_flash_sort: " << pair_cases << " dfa_state_pair cases matched std::sort, sentinels included" << std::endl;

  run_bucket_sort_tests();
}

// Benchmark against today's hashed path on a cram layer 40 like input: shape
// 2, 8 byte ids below 4479239467, about a third of the pairs distinct, most
// rows rejecting on their first transition.

struct HashRecord
{
  uint32_t data[4];

  bool operator<(const HashRecord& b) const
  {
    return std::lexicographical_compare(data, data + 3, b.data, b.data + 3);
  }

  bool key_equal(const HashRecord& b) const
  {
    return std::equal(data, data + 3, b.data);
  }
};

static double seconds_since(std::chrono::steady_clock::time_point start)
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

static void run_bench(size_t n)
{
  const int shape = 2;
  typedef RowRankRecord<uint64_t, shape, uint64_t> record_t;

  if(n > UINT32_MAX)
    {
      throw std::invalid_argument("bench n must fit 32 bit ranks");
    }

  std::cout << "bench n=" << n << " shape=" << shape << " id_bytes=8" << std::endl;

  std::mt19937_64 rng(1);
  size_t distinct_pool = std::max(n / 3, size_t(1));
  std::vector<uint64_t> pool(distinct_pool * shape);
  for(size_t i = 0; i < distinct_pool; ++i)
    {
      bool reject_first = rng() % 10 < 3;
      pool[i * shape] = reject_first ? 0 : 2 + rng() % 4479239465ULL;
      pool[i * shape + 1] = 2 + rng() % 4479239465ULL;
    }
  std::vector<uint64_t> transitions(n * shape);
  for(size_t i = 0; i < n; ++i)
    {
      size_t p = rng() % distinct_pool;
      transitions[i * shape] = pool[p * shape];
      transitions[i * shape + 1] = pool[p * shape + 1];
    }
  pool.clear();
  pool.shrink_to_fit();

  // today: SHA-256 per row, sort the hash records, check equal keys by memcmp
  auto start = std::chrono::steady_clock::now();
  std::vector<HashRecord> hashed(n);
  std::vector<size_t> indexes(n);
  for(size_t i = 0; i < n; ++i)
    {
      indexes[i] = i;
    }
  auto hash_row = [&](size_t i)
  {
    unsigned char hash_output[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX *context = EVP_MD_CTX_create();
    bool ok = ((context != NULL) &&
               (EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1) &&
               (EVP_DigestUpdate(context, &transitions[i * shape], shape * sizeof(uint64_t)) == 1) &&
               (EVP_DigestFinal_ex(context, hash_output, 0) == 1));
    EVP_MD_CTX_destroy(context);
    if(!ok)
      {
        throw std::runtime_error("SHA-256 failed");
      }
    std::memcpy(hashed[i].data, hash_output, 3 * sizeof(uint32_t));
    hashed[i].data[3] = uint32_t(i);
  };
  TRY_PARALLEL_3(std::for_each, indexes.begin(), indexes.end(), hash_row);
  indexes.clear();
  indexes.shrink_to_fit();
  double hash_seconds = seconds_since(start);

  start = std::chrono::steady_clock::now();
  parallel_sort_in_place(hashed.data(), hashed.data() + n);
  double hash_sort_seconds = seconds_since(start);

  start = std::chrono::steady_clock::now();
  auto collision = TRY_PARALLEL_3(std::adjacent_find, hashed.begin(), hashed.end(), [&](const HashRecord& a, const HashRecord& b)
  {
    if(!a.key_equal(b))
      {
        return false;
      }
    return std::memcmp(&transitions[size_t(a.data[3]) * shape], &transitions[size_t(b.data[3]) * shape], shape * sizeof(uint64_t)) != 0;
  });
  if(collision != hashed.end())
    {
      throw std::logic_error("bench: hash collision");
    }
  size_t hash_distinct = 0;
  for(size_t i = 0; i < n; ++i)
    {
      if((i == 0) || !hashed[i - 1].key_equal(hashed[i]))
        {
          ++hash_distinct;
        }
    }
  double hash_check_seconds = seconds_since(start);
  hashed.clear();
  hashed.shrink_to_fit();

  // proposed: records carry their rows, sorted by them
  start = std::chrono::steady_clock::now();
  std::vector<record_t> records(n);
  for(size_t i = 0; i < n; ++i)
    {
      records[i].row[0] = transitions[i * shape];
      records[i].row[1] = transitions[i * shape + 1];
      records[i].rank = i;
    }
  double fill_seconds = seconds_since(start);

  start = std::chrono::steady_clock::now();
  flash_sort(records.data(), records.data() + n);
  double flash_seconds = seconds_since(start);

  start = std::chrono::steady_clock::now();
  size_t flash_distinct = 0;
  for(size_t i = 0; i < n; ++i)
    {
      if((i == 0) || !records[i - 1].row_equal(records[i]))
        {
          ++flash_distinct;
        }
    }
  double flash_count_seconds = seconds_since(start);

  if(flash_distinct != hash_distinct)
    {
      throw std::logic_error("bench: flash sort found " + std::to_string(flash_distinct) + " distinct rows, hashing " + std::to_string(hash_distinct));
    }

  std::cout << "hashed: hash " << hash_seconds << "s, sort " << hash_sort_seconds << "s, check and count " << hash_check_seconds << "s, total " << (hash_seconds + hash_sort_seconds + hash_check_seconds) << "s" << std::endl;
  std::cout << "flash: fill " << fill_seconds << "s, sort " << flash_seconds << "s, count " << flash_count_seconds << "s, total " << (fill_seconds + flash_seconds + flash_count_seconds) << "s" << std::endl;
  std::cout << "both found " << flash_distinct << " distinct rows of " << n << std::endl;
}

// Page-ins since boot, from the host's VM statistics, or 0 where this has
// no way to read them.
static uint64_t host_pageins()
{
#ifndef __APPLE__
  return 0;
#else
  vm_statistics64_data_t stats;
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  if(host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count) != KERN_SUCCESS)
    {
      throw std::runtime_error("host_statistics64 failed");
    }
  return stats.pageins;
#endif
}

static uint64_t splitmix64(uint64_t x)
{
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// Fills the rows run_bench_file uses into records.
template <class record_t>
static void bench_file_fill(record_t *records, size_t n)
{
  size_t distinct_pool = std::max(n / 3, size_t(1));
  size_t chunk_count = 4096;
  size_t chunk_size = (n + chunk_count - 1) / chunk_count;
  std::vector<size_t> chunks(chunk_count);
  for(size_t c = 0; c < chunk_count; ++c)
    {
      chunks[c] = c;
    }
  auto fill_chunk = [&](size_t c)
  {
    std::mt19937_64 rng(c);
    for(size_t i = std::min(n, c * chunk_size); i < std::min(n, (c + 1) * chunk_size); ++i)
      {
        uint64_t p = rng() % distinct_pool;
        uint64_t h0 = splitmix64(2 * p);
        uint64_t h1 = splitmix64(2 * p + 1);
        records[i].row[0] = (h0 % 10 < 3) ? 0 : 2 + (h0 >> 8) % 4479239465ULL;
        records[i].row[1] = 2 + h1 % 4479239465ULL;
        records[i].rank = i;
      }
  };
  TRY_PARALLEL_3(std::for_each, chunks.begin(), chunks.end(), fill_chunk);
}

// run_bench_file's rows sorted by bucket_sort, through a MemoryMap of the
// same file.
static void run_bench_file_bucket(const std::string& path, size_t n)
{
  const int shape = 2;
  typedef RowRankRecord<uint64_t, shape, uint64_t> record_t;

  auto phase_start = std::chrono::steady_clock::now();
  uint64_t pageins_start = host_pageins();
  auto end_phase = [&](const std::string& phase)
  {
    double elapsed = seconds_since(phase_start);
    uint64_t pageins = host_pageins() - pageins_start;
    std::cout << phase << ": " << elapsed << "s, " << pageins << " page-ins (" << (elapsed > 0 ? double(pageins) / elapsed : 0.0) << "/s)" << std::endl;
    phase_start = std::chrono::steady_clock::now();
    pageins_start = host_pageins();
  };

  MemoryMap<record_t> map(path, n);
  bench_file_fill(map.begin(), n);
  end_phase("fill");

  // ids below 4479239467 use the low 33 bits of transition 0
  int digit_bit = bucket_digit_bit(64, 4479239467ULL);
  BucketSortStats stats = bucket_sort(map, digit_bit);
  end_phase(std::string("sort and checks (") + (stats.out_of_core ? "out of core" : "in memory") + ")");

  const record_t *records = map.begin();
  size_t distinct = 0;
  for(size_t i = 0; i < n; ++i)
    {
      if((i == 0) || !records[i - 1].row_equal(records[i]))
        {
          ++distinct;
        }
    }
  end_phase("count");
  std::cout << distinct << " distinct rows of " << n << std::endl;

  map.unlink();
  std::cout << "removed " << path << std::endl;
}

// Out of core: the same layer 40 like rows as run_bench, as records in a
// file mapped shared, sorted in place by FlashSort, or by bucket_sort when
// bucket is set. Rows are a function of a random pool index, so the pool of
// distinct rows needs no memory.
static void run_bench_file(const std::string& path, size_t n, bool bucket)
{
  const int shape = 2;
  typedef RowRankRecord<uint64_t, shape, uint64_t> record_t;

  size_t bytes = n * sizeof(record_t);
  std::cout << "bench_file " << path << " n=" << n << " bytes=" << bytes << " physical memory=" << physical_memory_bytes() << " sort=" << (bucket ? "bucket_sort" : "FlashSort") << std::endl;
  if(bucket)
    {
      run_bench_file_bucket(path, n);
      return;
    }

  int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
  if(fd < 0)
    {
      throw std::runtime_error("open " + path + " failed");
    }
  if(::ftruncate(fd, off_t(bytes)) != 0)
    {
      throw std::runtime_error("ftruncate " + path + " failed");
    }
  void *mapped = ::mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(mapped == MAP_FAILED)
    {
      throw std::runtime_error("mmap " + path + " failed");
    }
  ::close(fd);
  record_t *records = static_cast<record_t *>(mapped);

  auto phase_start = std::chrono::steady_clock::now();
  uint64_t pageins_start = host_pageins();
  auto end_phase = [&](const std::string& phase)
  {
    double elapsed = seconds_since(phase_start);
    uint64_t pageins = host_pageins() - pageins_start;
    std::cout << phase << ": " << elapsed << "s, " << pageins << " page-ins (" << (elapsed > 0 ? double(pageins) / elapsed : 0.0) << "/s)" << std::endl;
    phase_start = std::chrono::steady_clock::now();
    pageins_start = host_pageins();
  };

  bench_file_fill(records, n);
  end_phase("fill");

  FlashSort<record_t>(32, size_t(1) << 20, n / 256).sort(records, records + n);
  end_phase("sort and is_sorted check");

  size_t distinct = 0;
  for(size_t i = 0; i < n; ++i)
    {
      if((i == 0) || !records[i - 1].row_equal(records[i]))
        {
          ++distinct;
        }
    }
  end_phase("count");
  std::cout << distinct << " distinct rows of " << n << std::endl;

  ::munmap(mapped, bytes);
  if(::unlink(path.c_str()) != 0)
    {
      throw std::runtime_error("unlink " + path + " failed");
    }
  std::cout << "removed " << path << std::endl;
}

// Benchmark against parallel_sort on forward pass like pairs: narrow states
// below 3.5e9 on each side, each distinct pair about twice, 0.04% of them
// the sentinel. Every sort starts from the same shuffled input, and each
// result must equal the first.
static void run_bench_pairs(size_t n)
{
  typedef dfa_state_pair<uint32_t> pair_t;

  std::cout << "bench_pairs n=" << n << " (" << n * sizeof(pair_t) << " bytes), physical memory=" << physical_memory_bytes() << std::endl;

  std::mt19937_64 rng(2);
  std::vector<pair_t> input(n);
  for(size_t i = 0; i < n; i += 2)
    {
      pair_t pair(rng() % 3500000000ULL, rng() % 3500000000ULL);
      input[i] = pair;
      if(i + 1 < n)
        {
          input[i + 1] = pair;
        }
    }
  for(size_t i = 0; i < n; i += 2500)
    {
      input[i] = pair_t(UINT32_MAX, UINT32_MAX);
    }
  std::shuffle(input.begin(), input.end(), rng);

  std::vector<pair_t> first;
  auto run = [&](const std::string& name, auto sort_func)
  {
    std::vector<pair_t> working = input;
    auto start = std::chrono::steady_clock::now();
    sort_func(working.data(), working.data() + n);
    double elapsed = seconds_since(start);
    if(first.size() == 0)
      {
        first.swap(working);
      }
    else if(working != first)
      {
        throw std::logic_error("bench_pairs: " + name + " result differs");
      }
    std::cout << name << ": " << elapsed << "s" << std::endl;
  };

  for(int round = 0; round < 2; ++round)
    {
      run("parallel_sort_in_place", [](pair_t *begin, pair_t *end) {parallel_sort_in_place(begin, end);});
      run("std::sort(par)", [](pair_t *begin, pair_t *end) {TRY_PARALLEL_2(std::sort, begin, end);});
      run("FlashSort", [](pair_t *begin, pair_t *end) {FlashSort<pair_t>().sort(begin, end);});
    }
  std::cout << "all sorts agreed on " << n << " pairs" << std::endl;
}

int main(int argc, char **argv)
{
  if((argc == 3) && (std::string(argv[1]) == "bench"))
    {
      run_bench(std::stoull(argv[2]));
      return 0;
    }

  if((argc == 3) && (std::string(argv[1]) == "bench_pairs"))
    {
      run_bench_pairs(std::stoull(argv[2]));
      return 0;
    }

  if((argc == 4) && (std::string(argv[1]) == "bench_file"))
    {
      run_bench_file(argv[2], std::stoull(argv[3]), false);
      return 0;
    }

  if((argc == 4) && (std::string(argv[1]) == "bench_bucket_file"))
    {
      run_bench_file(argv[2], std::stoull(argv[3]), true);
      return 0;
    }

  if(argc != 1)
    {
      std::cerr << "usage: " << argv[0] << " [bench N | bench_pairs N | bench_file PATH N | bench_bucket_file PATH N]" << std::endl;
      return 1;
    }

  run_tests();
  return 0;
}
