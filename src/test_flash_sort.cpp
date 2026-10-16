// test_flash_sort.cpp

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
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

// Out of core: the same layer 40 like rows as run_bench, as records in a
// file mapped shared, sorted in place. Rows are a function of a random pool
// index, so the pool of distinct rows needs no memory.
static void run_bench_file(const std::string& path, size_t n)
{
  const int shape = 2;
  typedef RowRankRecord<uint64_t, shape, uint64_t> record_t;

  size_t bytes = n * sizeof(record_t);
  std::cout << "bench_file " << path << " n=" << n << " bytes=" << bytes << " physical memory=" << physical_memory_bytes() << std::endl;

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

  size_t distinct_pool = std::max(n / 3, size_t(1));

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
      run_bench_file(argv[2], std::stoull(argv[3]));
      return 0;
    }

  if(argc != 1)
    {
      std::cerr << "usage: " << argv[0] << " [bench N | bench_pairs N | bench_file PATH N]" << std::endl;
      return 1;
    }

  run_tests();
  return 0;
}
