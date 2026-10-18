// bucket_sort.h

#ifndef BUCKET_SORT_H
#define BUCKET_SORT_H

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "MemoryMap.h"
#include "Profile.h"
#include "flash_sort.h"
#include "parallel.h"
#include "utils.h"

// Sort of a file of records by key, in sequential passes over files.
//
// A file at or under the cutoff is sorted in place by FlashSort, as before.
// A larger one is partitioned into bucket files by a digit of its key; each
// bucket is loaded, sorted in memory and written back over the input in
// bucket order. The data is read twice and written twice, in large
// sequential transfers.
//
// Anonymous memory is held only within the in-core limit, counting
// everything held at once: partition buffers and blocks, then the buckets
// in memory together. A bucket larger than the limit is never loaded. It is
// partitioned again from its own file, on the digit after the key bits all
// its records share, which the partition pass records as it goes. A bucket
// whose keys are all equal cannot be split, and needs no sort; it is passed
// on in order, in pieces within the limit.
//
// The caller names the first digit: digit_bit is the first key bit that
// may be set, a claim the partition pass checks on every record. Each
// record's key bits before digit_bit must be zero, so a bound the caller
// already knows (a state below its layer's size, a rank below the pair
// count) places the digit without reading the data first. Deeper levels
// check the same way against their bucket's shared prefix. An optional
// sentinel record is exempt and goes to one extra bucket after the others,
// which suits the forward pass's filtered pairs, whose states are the
// type's maximum.
//
// Records are T with FlashSort's key interface (key_bits, key_byte_at_bit,
// first_difference_bit, key_less), and a value initialized T must have an
// all zero key.

// The in-core limit on anonymous memory, in bytes. 0, the production
// value, means DFA_BUCKET_SORT_MEMORY_MAX if set (a positive number of
// bytes; anything else throws), else 1/4 of physical memory, the share
// parallel_sort allows its buffer. Tests lower it.
extern size_t bucket_sort_memory_max;

// Files of more than this many bytes take the bucket path. 0, the
// production value, means DFA_BUCKET_SORT_MIN_BYTES if set (a positive
// number of bytes; anything else throws), else 1/10 of physical memory:
// on 128 GiB, RegionSort over a file mapping and the bucket path tie at
// about 12 GB of rows, RegionSort is 4 times faster at 8 GB, and the
// bucket path 2 times faster at 16 GB and 5 to 7 at 32 to 96 GB. Tests
// lower it.
extern size_t bucket_sort_min_bytes;

size_t bucket_sort_memory_bytes();
size_t bucket_sort_cutoff_bytes();
bool bucket_sort_out_of_core(size_t bytes);

// The first key bit that may be set when a key's leading field, field_bits
// wide, holds values below bound.
int bucket_digit_bit(int field_bits, uint64_t bound);

// Opens path for writing, writes bytes at offset, and closes it, checking
// each step.
void bucket_append_bytes(const std::string& path, const void *buffer, size_t bytes, size_t offset);

// What a bucket_sort did, for its log lines and for tests.
struct BucketSortStats
{
  bool out_of_core = false;
  size_t buckets = 0;
  size_t buckets_used = 0;
  // buckets over the in-core limit partitioned again, at any depth
  size_t repartitioned = 0;
  // buckets over the in-core limit whose keys were all equal
  size_t equal_runs = 0;
  size_t depth = 0;
  size_t boundaries = 0;
};

// How records map to buckets: the digit_width key bits from digit_bit, for
// records whose key bits before digit_bit match prefix.
template <class T>
struct BucketSpec
{
  static constexpr size_t invalid_bucket = SIZE_MAX;

  int digit_bit;
  int digit_width;
  T prefix;
  std::optional<T> sentinel;

  size_t digit_buckets() const
  {
    return size_t(1) << digit_width;
  }

  size_t bucket_count() const
  {
    return digit_buckets() + (sentinel ? 1 : 0);
  }

  // The record's bucket, or invalid_bucket if its key differs from prefix
  // before digit_bit.
  size_t bucket_of(const T& record) const
  {
    if(sentinel && (record.first_difference_bit(*sentinel, 0) >= T::key_bits))
      {
        return digit_buckets();
      }
    if((digit_bit > 0) && (record.first_difference_bit(prefix, 0) < digit_bit))
      {
        return invalid_bucket;
      }

    uint32_t high = record.key_byte_at_bit(digit_bit);
    if(digit_width <= 8)
      {
        return size_t(high >> (8 - digit_width));
      }
    uint32_t low = (digit_bit + 8 < T::key_bits) ? record.key_byte_at_bit(digit_bit + 8) : 0;
    return size_t(((high << 8) | low) >> (16 - digit_width));
  }
};

// Digit width for count records of T from digit_bit: the fewest bits, up
// to 12, whose buckets average at most an eighth of the in-core limit, and
// never past the end of the key.
template <class T>
BucketSpec<T> make_bucket_spec(size_t count, int digit_bit, const T& prefix, std::optional<T> sentinel)
{
  if((digit_bit < 0) || (digit_bit >= T::key_bits))
    {
      throw std::invalid_argument(std::format("make_bucket_spec: digit_bit {:d} outside the {:d} bit key", digit_bit, T::key_bits));
    }

  size_t bucket_target = std::max(bucket_sort_memory_bytes() / 8, sizeof(T));
  size_t bytes = count * sizeof(T);
  int width = 1;
  while((width < 12) && (width < T::key_bits - digit_bit) && (bytes / (size_t(1) << width) > bucket_target))
    {
      ++width;
    }
  return BucketSpec<T>{digit_bit, width, prefix, sentinel};
}

// Bucket files written by a BucketAppender, one per bucket that holds any
// records, named <dir>/<prefix>.<bucket>, with each bucket's record count,
// a record from it, and the number of leading key bits all its records
// share with that record. Bucket files still present when it is destroyed
// are unlinked.
template <class T>
class BucketFiles
{
  std::string dir;
  std::string prefix;

public:

  BucketSpec<T> spec;
  std::vector<size_t> counts;
  std::vector<T> samples;
  std::vector<int> shared_bits;

  BucketFiles(std::string dir_in, std::string prefix_in, BucketSpec<T> spec_in, std::vector<size_t> counts_in, std::vector<T> samples_in, std::vector<int> shared_bits_in)
    : dir(dir_in),
      prefix(prefix_in),
      spec(spec_in),
      counts(counts_in),
      samples(samples_in),
      shared_bits(shared_bits_in)
  {
  }

  BucketFiles(const BucketFiles&) = delete;
  BucketFiles& operator=(const BucketFiles&) = delete;

  BucketFiles(BucketFiles&& other)
    : dir(std::move(other.dir)),
      prefix(std::move(other.prefix)),
      spec(other.spec),
      counts(std::move(other.counts)),
      samples(std::move(other.samples)),
      shared_bits(std::move(other.shared_bits))
  {
    other.counts.clear();
  }

  ~BucketFiles()
  {
    for(size_t b = 0; b < counts.size(); ++b)
      {
        if(counts[b] > 0)
          {
            // a file already unlinked is fine; nothing else can be done here
            ::unlink(path(b).c_str());
          }
      }
  }

  const std::string& directory() const
  {
    return dir;
  }

  std::string name(size_t bucket) const
  {
    return prefix + "." + std::to_string(bucket);
  }

  std::string path(size_t bucket) const
  {
    return dir + "/" + name(bucket);
  }

  size_t total() const
  {
    size_t sum = 0;
    for(size_t count : counts)
      {
        sum += count;
      }
    return sum;
  }

  // Unlinks one bucket's file, which must exist.
  void unlink_bucket(size_t bucket)
  {
    if(::unlink(path(bucket).c_str()) != 0)
      {
        throw std::system_error(errno, std::generic_category(), "BucketFiles: unlink " + path(bucket));
      }
    counts[bucket] = 0;
  }
};

// Appends records to bucket files from many threads. Each thread holds a
// buffer per bucket; a full buffer reserves the next range of its bucket's
// file with an atomic add and writes it there, so writers never share
// bytes. Each thread also tracks, per bucket, a record and the key bits its
// bucket's records share with it. finish() checks every file's length
// against what was reserved for it, and returns the bucket files.
template <class T>
class BucketAppender
{
  std::string dir;
  std::string prefix;
  BucketSpec<T> spec;
  size_t buffer_records;
  std::vector<std::atomic<size_t>> reserved;
  std::atomic<bool> finished;

  // per writer, per bucket: a record seen, and the bits shared with it
  std::mutex writers_mutex;
  std::vector<std::vector<std::optional<T>>> writer_samples;
  std::vector<std::vector<int>> writer_shared;

  std::string path(size_t bucket) const
  {
    return dir + "/" + prefix + "." + std::to_string(bucket);
  }

  // Merges the writers' samples: a record agreeing with its writer's
  // sample up to bit i, and that sample agreeing with the bucket's up to
  // bit j, agrees with the bucket's up to min(i, j).
  void merge_samples(std::vector<T>& samples, std::vector<int>& shared) const
  {
    size_t buckets = spec.bucket_count();
    samples.assign(buckets, T());
    shared.assign(buckets, T::key_bits);
    for(size_t b = 0; b < buckets; ++b)
      {
        std::optional<T> chosen;
        for(size_t w = 0; w < writer_samples.size(); ++w)
          {
            if(!writer_samples[w][b])
              {
                continue;
              }
            if(!chosen)
              {
                chosen = writer_samples[w][b];
              }
            shared[b] = std::min({shared[b], writer_shared[w][b], writer_samples[w][b]->first_difference_bit(*chosen, 0)});
          }
        if(chosen)
          {
            samples[b] = *chosen;
          }
      }
  }

public:

  // One thread's buffers.
  class Writer
  {
    BucketAppender& appender;
    std::vector<std::vector<T>> buffers;
    std::vector<std::optional<T>> samples;
    std::vector<int> shared;

  public:

    explicit Writer(BucketAppender& appender_in)
      : appender(appender_in),
        buffers(appender_in.spec.bucket_count()),
        samples(appender_in.spec.bucket_count()),
        shared(appender_in.spec.bucket_count(), T::key_bits)
    {
    }

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    void append(size_t bucket, const T& record)
    {
      assert(bucket < buffers.size());
      if(!samples[bucket])
        {
          samples[bucket] = record;
        }
      else if(shared[bucket] > 0)
        {
          shared[bucket] = std::min(shared[bucket], record.first_difference_bit(*samples[bucket], 0));
        }

      std::vector<T>& buffer = buffers[bucket];
      if(buffer.capacity() == 0)
        {
          buffer.reserve(appender.buffer_records);
        }
      buffer.push_back(record);
      if(buffer.size() >= appender.buffer_records)
        {
          flush(bucket);
        }
    }

    void flush(size_t bucket)
    {
      std::vector<T>& buffer = buffers[bucket];
      if(buffer.empty())
        {
          return;
        }
      size_t start = appender.reserved[bucket].fetch_add(buffer.size());
      bucket_append_bytes(appender.path(bucket), buffer.data(), buffer.size() * sizeof(T), start * sizeof(T));
      buffer.clear();
    }

    // Writes every buffer and hands this writer's samples to the appender.
    void flush_all()
    {
      for(size_t b = 0; b < buffers.size(); ++b)
        {
          flush(b);
        }
      std::lock_guard<std::mutex> lock(appender.writers_mutex);
      appender.writer_samples.push_back(samples);
      appender.writer_shared.push_back(shared);
    }
  };

  // buffer_bytes_total is the anonymous memory all writers' buffers may
  // hold together.
  BucketAppender(std::string dir_in, std::string prefix_in, BucketSpec<T> spec_in, size_t writers, size_t buffer_bytes_total)
    : dir(dir_in),
      prefix(prefix_in),
      spec(spec_in),
      reserved(spec_in.bucket_count()),
      finished(false)
  {
    // Each buffer is at most 4MB. A buffer is allocated when its bucket
    // first gets a record.
    size_t buffer_bytes = buffer_bytes_total / std::max(writers, size_t(1)) / spec.bucket_count();
    buffer_bytes = std::clamp(buffer_bytes, sizeof(T), size_t(1) << 22);
    buffer_records = std::max(buffer_bytes / sizeof(T), size_t(1));

    for(size_t b = 0; b < spec.bucket_count(); ++b)
      {
        struct stat stat_buffer;
        if(::stat(path(b).c_str(), &stat_buffer) == 0)
          {
            throw std::logic_error("BucketAppender: " + path(b) + " already exists");
          }
      }
  }

  // The bucket files written so far, unchecked, for removal after a
  // failure.
  BucketFiles<T> abandon()
  {
    finished = true;
    std::vector<size_t> counts(spec.bucket_count());
    for(size_t b = 0; b < counts.size(); ++b)
      {
        counts[b] = reserved[b].load();
      }
    return BucketFiles<T>(dir, prefix, spec, counts, std::vector<T>(counts.size()), std::vector<int>(counts.size(), 0));
  }

  // Call after every Writer has called flush_all().
  BucketFiles<T> finish()
  {
    if(finished.exchange(true))
      {
        throw std::logic_error("BucketAppender: finish called twice");
      }

    std::vector<size_t> counts(spec.bucket_count());
    for(size_t b = 0; b < counts.size(); ++b)
      {
        counts[b] = reserved[b].load();
      }
    std::vector<T> samples;
    std::vector<int> shared;
    merge_samples(samples, shared);
    // Owns the files from here, so a failed check below removes them.
    BucketFiles<T> files(dir, prefix, spec, counts, samples, shared);

    for(size_t b = 0; b < counts.size(); ++b)
      {
        if(counts[b] == 0)
          {
            continue;
          }
        struct stat stat_buffer;
        if(::stat(path(b).c_str(), &stat_buffer) != 0)
          {
            throw std::system_error(errno, std::generic_category(), "BucketAppender: stat " + path(b));
          }
        if(size_t(stat_buffer.st_size) != counts[b] * sizeof(T))
          {
            throw std::runtime_error(std::format("BucketAppender: {:s} is {:d} bytes, but {:d} records of {:d} bytes were reserved", path(b), stat_buffer.st_size, counts[b], sizeof(T)));
          }
      }

    return files;
  }
};

// Partitions the records of the file at path into bucket files under dir.
// Threads each read their own blocks of the file sequentially. Throws if a
// record's key differs from spec.prefix before spec.digit_bit, naming the
// record. Buffers hold at most half the in-core limit and blocks a quarter.
template <class T>
BucketFiles<T> bucket_partition_file(const std::string& path, size_t count, const std::string& dir, const std::string& prefix, const BucketSpec<T>& spec)
{
  size_t threads = std::max(size_t(std::thread::hardware_concurrency()), size_t(1));
  size_t memory = bucket_sort_memory_bytes();
  BucketAppender<T> appender(dir, prefix, spec, threads, memory / 2);

  int fd = ::open(path.c_str(), O_RDONLY);
  if(fd < 0)
    {
      throw std::system_error(errno, std::generic_category(), "bucket_partition_file: open " + path);
    }

  // Blocks of up to 64MB, smaller when the limit is, in whole records.
  size_t block_records = std::max(std::min(size_t(1) << 26, memory / 4 / threads) / sizeof(T), size_t(1));
  size_t block_count = (count + block_records - 1) / block_records;
  std::atomic<size_t> next_block(0);
  std::atomic<bool> failed(false);
  std::exception_ptr failure;
  std::mutex failure_mutex;

  auto work = [&]()
  {
    try
      {
        typename BucketAppender<T>::Writer writer(appender);
        std::vector<T> block(std::min(block_records, count));
        for(size_t k = next_block++; (k < block_count) && !failed; k = next_block++)
          {
            size_t begin = k * block_records;
            size_t n = std::min(block_records, count - begin);
            pread_bytes(fd, block.data(), n * sizeof(T), begin * sizeof(T));
            for(size_t i = 0; i < n; ++i)
              {
                size_t bucket = spec.bucket_of(block[i]);
                if(bucket == BucketSpec<T>::invalid_bucket)
                  {
                    throw std::logic_error(std::format("bucket_sort: record {:d} of {:s} differs from its bucket's prefix before bit {:d}", begin + i, path, spec.digit_bit));
                  }
                writer.append(bucket, block[i]);
              }
          }
        writer.flush_all();
      }
    catch(...)
      {
        std::lock_guard<std::mutex> lock(failure_mutex);
        if(!failure)
          {
            failure = std::current_exception();
          }
        failed = true;
      }
  };

  std::vector<std::thread> workers;
  for(size_t t = 0; t < threads; ++t)
    {
      workers.emplace_back(work);
    }
  for(std::thread& worker : workers)
    {
      worker.join();
    }

  if(failure)
    {
      // removes whatever was written
      BucketFiles<T> abandoned = appender.abandon();
      ::close(fd);
      std::rethrow_exception(failure);
    }

  if(::close(fd) != 0)
    {
      BucketFiles<T> abandoned = appender.abandon();
      throw std::system_error(errno, std::generic_category(), "bucket_partition_file: close " + path);
    }

  BucketFiles<T> files = appender.finish();

  if(files.total() != count)
    {
      throw std::logic_error(std::format("bucket_sort: {:d} records partitioned into buckets of {:s}, expected {:d}", files.total(), path, count));
    }
  return files;
}

// Partitions count records, record i being populate(i), into bucket files
// under dir, for the file at path, which names them in messages. Records are made in chunks of up to 1GB (a quarter of the
// in-core limit when that is less), in parallel as MemoryMap's populate
// constructor makes them, and each chunk is appended to the buckets by
// threads each taking a part of it. Throws if a record's key differs from
// spec.prefix before spec.digit_bit, naming the record.
template <class T>
BucketFiles<T> bucket_partition_populate(const std::string& path, size_t count, const std::function<T(size_t)>& populate, const std::string& dir, const std::string& prefix, const BucketSpec<T>& spec)
{
  size_t threads = std::max(size_t(std::thread::hardware_concurrency()), size_t(1));
  size_t memory = bucket_sort_memory_bytes();
  BucketAppender<T> appender(dir, prefix, spec, threads, memory / 2);

  size_t chunk_records = std::max(std::min(size_t(1) << 30, memory / 4) / sizeof(T), size_t(1));
  std::vector<T> chunk(std::min(chunk_records, count));

  std::vector<std::unique_ptr<typename BucketAppender<T>::Writer>> writers;
  for(size_t t = 0; t < threads; ++t)
    {
      writers.push_back(std::make_unique<typename BucketAppender<T>::Writer>(appender));
    }

  std::exception_ptr failure;
  std::mutex failure_mutex;
  auto record_failure = [&]()
  {
    std::lock_guard<std::mutex> lock(failure_mutex);
    if(!failure)
      {
        failure = std::current_exception();
      }
  };

  for(size_t chunk_start = 0; (chunk_start < count) && !failure; chunk_start += chunk_records)
    {
      size_t n = std::min(chunk_records, count - chunk_start);

      // Each element's index comes from its address, as in MemoryMap's
      // populate constructor, over the vector's own random access iterators.
      T *chunk_data = chunk.data();
      try
        {
          TRY_PARALLEL_3(std::for_each,
                         chunk.begin(),
                         chunk.begin() + std::ptrdiff_t(n),
                         [&](T& element)
                         {
                           element = populate(chunk_start + size_t(&element - chunk_data));
                         });
        }
      catch(...)
        {
          record_failure();
          break;
        }

      size_t part = (n + threads - 1) / threads;
      std::vector<std::thread> workers;
      for(size_t t = 0; t < threads; ++t)
        {
          workers.emplace_back([&, t]()
          {
            try
              {
                for(size_t i = std::min(n, t * part); i < std::min(n, (t + 1) * part); ++i)
                  {
                    size_t bucket = spec.bucket_of(chunk[i]);
                    if(bucket == BucketSpec<T>::invalid_bucket)
                      {
                        throw std::logic_error(std::format("bucket_sort: record {:d} of {:s} differs from its bucket's prefix before bit {:d}", chunk_start + i, path, spec.digit_bit));
                      }
                    writers[t]->append(bucket, chunk[i]);
                  }
              }
            catch(...)
              {
                record_failure();
              }
          });
        }
      for(std::thread& worker : workers)
        {
          worker.join();
        }
    }

  if(!failure)
    {
      try
        {
          for(auto& writer : writers)
            {
              writer->flush_all();
            }
        }
      catch(...)
        {
          record_failure();
        }
    }

  if(failure)
    {
      // removes whatever was written
      BucketFiles<T> abandoned = appender.abandon();
      std::rethrow_exception(failure);
    }

  BucketFiles<T> files = appender.finish();
  if(files.total() != count)
    {
      throw std::logic_error(std::format("bucket_sort: {:d} records partitioned into buckets of {:s}, expected {:d}", files.total(), path, count));
    }
  return files;
}

// Reads records [begin, begin + n) of the file at path.
template <class T>
std::vector<T> bucket_read_records(const std::string& path, size_t begin, size_t n)
{
  std::vector<T> records(n);
  int fd = ::open(path.c_str(), O_RDONLY);
  if(fd < 0)
    {
      throw std::system_error(errno, std::generic_category(), "bucket_read_records: open " + path);
    }
  try
    {
      pread_bytes(fd, records.data(), n * sizeof(T), begin * sizeof(T));
    }
  catch(...)
    {
      ::close(fd);
      throw;
    }
  if(::close(fd) != 0)
    {
      throw std::system_error(errno, std::generic_category(), "bucket_read_records: close " + path);
    }
  return records;
}

// Calls process(offset, records) on each bucket of files, offset being the
// number of records before it, and then commit(offset, summary, records)
// with what process returned and the same records, in key order. process
// may reorder or overwrite records, which stay in memory until their
// commit. Buckets within the in-core limit are
// loaded and processed several at once, as many as fit the limit together
// at the largest such bucket's size. A bucket over the limit is partitioned
// again from its file, on the bits after those its records share, and its
// sub-buckets go through the same steps in its place. A bucket over the
// limit whose keys are all equal is passed on in order, in pieces within
// the limit. A bucket after one over the limit is not loaded until that one
// is handled, so nothing at this level is held while a level below runs.
// Each bucket file is unlinked once it has been handled.
template <class T, class Process, class Commit>
void for_each_bucket(BucketFiles<T>&& files, size_t base_offset, Process& process, Commit& commit, BucketSortStats& stats, size_t depth)
{
  typedef decltype(process(size_t(0), std::span<T>())) summary_t;

  stats.depth = std::max(stats.depth, depth);
  size_t memory = bucket_sort_memory_bytes();
  size_t bucket_count = files.counts.size();
  std::vector<size_t> offsets(bucket_count + 1, 0);
  size_t largest_loaded = 0;
  for(size_t b = 0; b < bucket_count; ++b)
    {
      offsets[b + 1] = offsets[b] + files.counts[b];
      if(files.counts[b] * sizeof(T) <= memory)
        {
          largest_loaded = std::max(largest_loaded, files.counts[b]);
        }
    }

  // The last bucket over the limit before each bucket, if any.
  std::vector<size_t> oversized_before(bucket_count, SIZE_MAX);
  for(size_t b = 1; b < bucket_count; ++b)
    {
      bool previous_over = (files.counts[b - 1] * sizeof(T) > memory);
      oversized_before[b] = previous_over ? (b - 1) : oversized_before[b - 1];
    }

  // Buckets loaded at once, and so held at once, fit the limit together.
  size_t largest_bytes = largest_loaded * sizeof(T);
  size_t workers = std::clamp(memory / std::max(largest_bytes, size_t(1)), size_t(1), size_t(4));

  std::atomic<size_t> next_bucket(0);
  std::mutex turn_mutex;
  std::condition_variable turn_changed;
  size_t next_turn = 0;
  std::atomic<bool> failed(false);
  std::exception_ptr failure;
  std::mutex failure_mutex;

  auto wait_turn = [&](size_t b)
  {
    std::unique_lock<std::mutex> lock(turn_mutex);
    turn_changed.wait(lock, [&]() {return (next_turn == b) || failed;});
    return !failed;
  };
  auto end_turn = [&]()
  {
    std::lock_guard<std::mutex> lock(turn_mutex);
    ++next_turn;
    turn_changed.notify_all();
  };

  auto handle = [&](size_t b)
  {
    size_t n = files.counts[b];
    size_t offset = base_offset + offsets[b];
    std::string path = files.path(b);

    if((n > 0) && (n * sizeof(T) > memory))
      {
        // Over the limit: handled on this bucket's turn, alone among this
        // level's buckets, since the levels below hold memory of their own.
        if(!wait_turn(b))
          {
            return;
          }
        if(files.shared_bits[b] >= T::key_bits)
          {
            ++stats.equal_runs;
            size_t piece = std::max(memory / sizeof(T), size_t(1));
            for(size_t begin = 0; begin < n; begin += piece)
              {
                size_t m = std::min(piece, n - begin);
                std::vector<T> records = bucket_read_records<T>(path, begin, m);
                summary_t summary = process(offset + begin, std::span<T>(records));
                commit(offset + begin, std::move(summary), std::span<T>(records));
              }
          }
        else
          {
            ++stats.repartitioned;
            BucketSpec<T> sub_spec = make_bucket_spec<T>(n, files.shared_bits[b], files.samples[b], std::nullopt);
            BucketFiles<T> sub_files = bucket_partition_file<T>(path, n, files.directory(), files.name(b), sub_spec);
            files.unlink_bucket(b);
            for_each_bucket<T>(std::move(sub_files), offset, process, commit, stats, depth + 1);
          }
        if(files.counts[b] > 0)
          {
            files.unlink_bucket(b);
          }
        end_turn();
        return;
      }

    if(oversized_before[b] != SIZE_MAX)
      {
        std::unique_lock<std::mutex> lock(turn_mutex);
        turn_changed.wait(lock, [&]() {return (next_turn > oversized_before[b]) || failed;});
        if(failed)
          {
            return;
          }
      }

    std::optional<summary_t> summary;
    std::vector<T> records;
    if(n > 0)
      {
        records = bucket_read_records<T>(path, 0, n);
        summary = process(offset, std::span<T>(records));
      }

    if(!wait_turn(b))
      {
        return;
      }
    if(n > 0)
      {
        commit(offset, std::move(*summary), std::span<T>(records));
        files.unlink_bucket(b);
      }
    end_turn();
  };

  auto work = [&]()
  {
    for(size_t b = next_bucket++; (b < bucket_count) && !failed; b = next_bucket++)
      {
        try
          {
            handle(b);
          }
        catch(...)
          {
            {
              std::lock_guard<std::mutex> lock(failure_mutex);
              if(!failure)
                {
                  failure = std::current_exception();
                }
            }
            {
              std::lock_guard<std::mutex> lock(turn_mutex);
              failed = true;
            }
            turn_changed.notify_all();
            return;
          }
      }
  };

  std::vector<std::thread> threads;
  for(size_t t = 0; t < workers; ++t)
    {
      threads.emplace_back(work);
    }
  for(std::thread& thread : threads)
    {
      thread.join();
    }

  if(failure)
    {
      std::rethrow_exception(failure);
    }
  if(next_turn != bucket_count)
    {
      throw std::logic_error(std::format("for_each_bucket: handled {:d} of {:d} buckets", next_turn, bucket_count));
    }
}

// Calls back on each sorted piece as bucket_sort_write_back writes it:
// sorted(offset, records) in parallel, once a piece is sorted, and
// committed(offset, records) in key order. Either may be empty.
template <class T>
struct BucketObserver
{
  std::function<void(size_t, std::span<const T>)> sorted;
  std::function<void(size_t, std::span<const T>)> committed;
};

// Sorts each piece in memory and writes it to the file at path at its
// offset, checking that each piece's first key is not less than the
// previous piece's last key. path must hold files.total() records' worth
// of bytes. Fills in the counts in stats. observer, if given, sees each
// sorted piece.
template <class T>
void bucket_sort_write_back(BucketFiles<T>&& files, const std::string& path, BucketSortStats& stats, const BucketObserver<T> *observer = nullptr)
{
  int fd = ::open(path.c_str(), O_WRONLY);
  if(fd < 0)
    {
      throw std::system_error(errno, std::generic_category(), "bucket_sort_write_back: open " + path);
    }

  std::optional<T> previous_last;
  size_t checked_boundaries = 0;
  size_t written = 0;

  auto process = [&](size_t offset, std::span<T> records)
  {
    FlashSort<T>().sort(records.data(), records.data() + records.size());
    pwrite_bytes(fd, records.data(), records.size() * sizeof(T), offset * sizeof(T));
    if(observer && observer->sorted)
      {
        observer->sorted(offset, std::span<const T>(records.data(), records.size()));
      }
    return std::make_tuple(records.front(), records.back(), records.size());
  };
  auto commit = [&](size_t offset, std::tuple<T, T, size_t> summary, std::span<T> records)
  {
    const auto& [first, last, n] = summary;
    if(offset != written)
      {
        throw std::logic_error(std::format("bucket_sort: piece at record {:d} of {:s} follows {:d} records written", offset, path, written));
      }
    if(previous_last && first.key_less(*previous_last))
      {
        throw std::logic_error(std::format("bucket_sort: piece at record {:d} of {:s} starts below the end of the piece before it", offset, path));
      }
    if(previous_last)
      {
        ++checked_boundaries;
      }
    previous_last = last;
    written += n;
    if(observer && observer->committed)
      {
        observer->committed(offset, std::span<const T>(records.data(), records.size()));
      }
  };

  size_t total = files.total();
  try
    {
      for_each_bucket<T>(std::move(files), 0, process, commit, stats, 0);
    }
  catch(...)
    {
      ::close(fd);
      throw;
    }

  if(::close(fd) != 0)
    {
      throw std::system_error(errno, std::generic_category(), "bucket_sort_write_back: close " + path);
    }
  if(written != total)
    {
      throw std::logic_error(std::format("bucket_sort: {:d} records written back to {:s}, expected {:d}", written, path, total));
    }
  stats.boundaries = checked_boundaries;
}

// The first digit's bit for a bound's digit_bit: a bound of 1 claims every
// key bit is zero, so the digit is the last bit, and every bit before it
// is still checked.
template <class T>
int bucket_first_digit_bit(int digit_bit)
{
  return std::min(digit_bit, T::key_bits - 1);
}

template <class T>
void bucket_log_partition(const std::string& path, size_t count, const BucketSpec<T>& spec, const BucketFiles<T>& files, BucketSortStats& stats)
{
  size_t largest = 0;
  stats.buckets = spec.bucket_count();
  for(size_t c : files.counts)
    {
      largest = std::max(largest, c);
      stats.buckets_used += (c > 0) ? 1 : 0;
    }
  std::cout << std::format("bucket_sort: {:d} records ({:d} bytes) of {:s} partitioned on key bits [{:d}, {:d}){:s} into {:d} of {:d} buckets, largest {:d} records ({:.1f}%), in-core limit {:d} bytes", count, count * sizeof(T), path, spec.digit_bit, spec.digit_bit + spec.digit_width, (spec.sentinel ? " plus a sentinel bucket" : ""), stats.buckets_used, stats.buckets, largest, 100.0 * double(largest) / double(std::max(count, size_t(1))), bucket_sort_memory_bytes()) << std::endl;
}

template <class T>
void bucket_log_sorted(size_t count, const BucketSortStats& stats)
{
  std::cout << std::format("bucket_sort: {:d} records sorted and written back in order, {:d} piece boundaries checked; {:d} buckets partitioned again, to depth {:d}, and {:d} of all equal keys", count, stats.boundaries, stats.repartitioned, stats.depth, stats.equal_runs) << std::endl;
}

// Records made by a populate function: in a mapped file, or in bucket
// files for the caller to pass to for_each_bucket.
template <class T>
struct PopulatedRecords
{
  std::optional<MemoryMap<T>> map;
  std::optional<BucketFiles<T>> buckets;
  // whether map is sorted by key
  bool sorted;
  BucketSortStats stats;
};

// Makes the file at path of count records, record i being populate(i),
// and calls after_populate once every record is made. At or under the
// cutoff this is MemoryMap's populate constructor, and the records are
// left in index order for the caller to sort. Above it, the records go
// straight into bucket files, after_populate runs, and the buckets are
// sorted and written to path, so the result is sorted, without the records
// ever being written to path in index order. With keep_buckets, the bucket
// files are returned instead, unsorted, for the caller to pass to
// for_each_bucket, and nothing is written to path. digit_bit and sentinel
// are as for bucket_sort.
template <class T, class AfterPopulate>
PopulatedRecords<T> bucket_populate(const std::string& path, size_t count, const std::function<T(size_t)>& populate, int digit_bit, std::optional<T> sentinel, AfterPopulate after_populate, bool keep_buckets = false)
{
  if((count == 0) || !bucket_sort_out_of_core(count * sizeof(T)))
    {
      PopulatedRecords<T> populated{MemoryMap<T>(path, count, populate), std::nullopt, false, BucketSortStats()};
      after_populate();
      return populated;
    }

  BucketSortStats stats;
  stats.out_of_core = true;

  Profile profile("bucket_populate");
  profile.tic("partition");

  BucketSpec<T> spec = make_bucket_spec<T>(count, bucket_first_digit_bit<T>(digit_bit), T(), sentinel);
  size_t slash = path.find_last_of('/');
  std::string dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
  std::string prefix = ((slash == std::string::npos) ? path : path.substr(slash + 1)) + ".bucket";

  BucketFiles<T> files = bucket_partition_populate<T>(path, count, populate, dir, prefix, spec);
  bucket_log_partition(path, count, spec, files, stats);

  profile.tic("after populate");

  after_populate();

  if(keep_buckets)
    {
      return PopulatedRecords<T>{std::nullopt, std::move(files), false, stats};
    }

  profile.tic("buckets");

  MemoryMap<T> map(path, count);
  map.munmap();
  bucket_sort_write_back(std::move(files), path, stats);

  profile.tic("mmap");

  map.mmap();
  bucket_log_sorted<T>(count, stats);
  return PopulatedRecords<T>{std::move(map), std::nullopt, true, stats};
}

// Sorts map's records by key. In memory with FlashSort at or under the
// cutoff, else through bucket files as described at the top, leaving map
// mapped as it was. digit_bit and sentinel are as for BucketSpec, with an
// all zero prefix.
template <class T>
BucketSortStats bucket_sort(MemoryMap<T>& map, int digit_bit, std::optional<T> sentinel = std::nullopt)
{
  BucketSortStats stats;
  size_t count = map.size();
  size_t bytes = count * sizeof(T);
  if(count == 0)
    {
      // an empty map is never mapped, and holds nothing to sort
      return stats;
    }
  if(!bucket_sort_out_of_core(bytes))
    {
      FlashSort<T>().sort(map.begin(), map.end());
      return stats;
    }
  stats.out_of_core = true;

  Profile profile("bucket_sort");
  profile.tic("partition");

  BucketSpec<T> spec = make_bucket_spec<T>(count, bucket_first_digit_bit<T>(digit_bit), T(), sentinel);
  std::string path = map.filename();
  size_t slash = path.find_last_of('/');
  std::string dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
  std::string prefix = ((slash == std::string::npos) ? path : path.substr(slash + 1)) + ".bucket";

  map.munmap();
  BucketFiles<T> files = bucket_partition_file<T>(path, count, dir, prefix, spec);

  bucket_log_partition(path, count, spec, files, stats);

  profile.tic("buckets");

  bucket_sort_write_back(std::move(files), path, stats);

  profile.tic("mmap");

  map.mmap();
  bucket_log_sorted<T>(count, stats);
  return stats;
}

// Sorted blocks: one way of handling records in key order, whether they
// fit the cutoff or not.
//
// A call site hands over records made by bucket_populate (or a
// RecordSink), and two callbacks over spans:
//
//   count(offset, block, before) -> size_t
//   emit(offset, block, before, base) -> size_t
//
// block is a run of records in key order starting at offset, before is the
// record just before it in key order (nullptr for the first), and base is
// the sum of the counts of every block before it. emit returns the count it
// produced, which must equal what count returned for the block. Counts must
// be additive: the count of a run cut into blocks, each given its before,
// is the same however it is cut.
//
// At or under the cutoff the records are one run, the populated file,
// sorted in place by FlashSort through its mapping. Above it each bucket
// is a run, loaded and sorted in memory as bucket_sort does. Either way a
// run is cut into blocks, and counts and emits run in parallel across
// blocks on the thread pool.

// Records per block for records of record_bytes.
inline size_t sorted_block_records(size_t record_bytes)
{
  size_t threads = std::max(size_t(std::thread::hardware_concurrency()), size_t(1));
  return std::clamp(bucket_sort_memory_bytes() / 4 / threads / (2 * record_bytes), size_t(1), size_t(1) << 20);
}

// Calls f(i) for i in [0, n) in parallel. An exception escaping a parallel
// algorithm terminates the process, so the first one is kept and thrown
// once every call has returned.
template <class F>
void bucket_parallel_for(size_t n, F f)
{
  std::vector<size_t> indexes(n);
  std::iota(indexes.begin(), indexes.end(), size_t(0));
  std::atomic<bool> failed(false);
  std::exception_ptr failure;
  std::mutex failure_mutex;
  auto body = [&](size_t i)
  {
    if(failed)
      {
        return;
      }
    try
      {
        f(i);
      }
    catch(...)
      {
        std::lock_guard<std::mutex> lock(failure_mutex);
        if(!failure)
          {
            failure = std::current_exception();
          }
        failed = true;
      }
  };
  TRY_PARALLEL_3(std::for_each, indexes.begin(), indexes.end(), body);
  if(failure)
    {
      std::rethrow_exception(failure);
    }
}

// A block of a run: where it starts in key order, its length, its count,
// and the counts of every block before it.
struct SortedBlock
{
  size_t offset;
  size_t n;
  size_t count;
  size_t base;
};

// What the sorted block handling saw, for checks and logs.
struct SortedBlocksStats
{
  bool out_of_core = false;
  // records passed to the callbacks, and sentinels set aside
  size_t records = 0;
  size_t sentinels = 0;
  size_t blocks = 0;
  // sum of every block's count
  size_t total = 0;
  BucketSortStats buckets;
};

// Cuts run, which starts at offset in key order, into blocks, and counts
// them in parallel, giving the first block before_run.
template <class T, class Count>
std::vector<SortedBlock> sorted_count_run(std::span<const T> run, size_t offset, const T *before_run, Count& count)
{
  size_t block_records = sorted_block_records(sizeof(T));
  size_t block_count = (run.size() + block_records - 1) / block_records;
  std::vector<SortedBlock> blocks(block_count);
  bucket_parallel_for(block_count, [&](size_t k)
  {
    size_t begin = k * block_records;
    size_t n = std::min(block_records, run.size() - begin);
    const T *before = (k > 0) ? &run[begin - 1] : before_run;
    blocks[k] = SortedBlock{offset + begin, n, count(offset + begin, run.subspan(begin, n), before), 0};
  });
  return blocks;
}

// Emits the blocks of run, which starts at blocks' first offset, in
// parallel, checking each emit's count against the block's.
template <class T, class Emit>
void sorted_emit_run(std::span<const T> run, const T *before_run, const std::vector<SortedBlock>& blocks, Emit& emit)
{
  size_t run_offset = blocks.empty() ? 0 : blocks.front().offset;
  bucket_parallel_for(blocks.size(), [&](size_t k)
  {
    const SortedBlock& block = blocks[k];
    size_t begin = block.offset - run_offset;
    const T *before = (k > 0) ? &run[begin - 1] : before_run;
    size_t emitted = emit(block.offset, run.subspan(begin, block.n), before, block.base);
    if(emitted != block.count)
      {
        throw std::logic_error(std::format("sorted blocks: block at record {:d} emitted {:d}, but counted {:d}", block.offset, emitted, block.count));
      }
  });
}

// Gives each block its base, continuing from base, and returns the total.
inline size_t sorted_assign_bases(std::vector<SortedBlock>& blocks, size_t base)
{
  for(SortedBlock& block : blocks)
    {
      block.base = base;
      base += block.count;
    }
  return base;
}

// Whether record has the sentinel's key.
template <class T>
bool sorted_is_sentinel(const T& record, const std::optional<T>& sentinel)
{
  return sentinel && (record.first_difference_bit(*sentinel, 0) >= T::key_bits);
}

// Sorts records by key and calls count and then emit on every block, as
// described above, once each, and unlinks the records' file or buckets.
// Records with the sentinel's key, which sort last, are set aside and
// counted rather than passed on.
template <class T, class Count, class Emit>
SortedBlocksStats sorted_emit(PopulatedRecords<T>&& records, std::optional<T> sentinel, Count count, Emit emit)
{
  SortedBlocksStats stats;

  if(records.map)
    {
      MemoryMap<T>& map = *records.map;
      size_t n = map.size();
      if(n > 0)
        {
          FlashSort<T>().sort(map.begin(), map.end());
          std::span<const T> all(map.begin(), n);
          size_t real = n;
          if(sentinel)
            {
              const T *first_sentinel = std::partition_point(all.data(), all.data() + n, [&](const T& record)
              {
                return record.key_less(*sentinel);
              });
              real = size_t(first_sentinel - all.data());
              for(size_t i = real; i < n; ++i)
                {
                  if(!sorted_is_sentinel(all[i], sentinel))
                    {
                      throw std::logic_error(std::format("sorted blocks: record {:d} of {:s} sorts after the sentinel", i, map.filename()));
                    }
                }
            }
          std::span<const T> run = all.subspan(0, real);
          std::vector<SortedBlock> blocks = sorted_count_run<T>(run, 0, nullptr, count);
          stats.total = sorted_assign_bases(blocks, 0);
          sorted_emit_run<T>(run, nullptr, blocks, emit);
          stats.records = real;
          stats.sentinels = n - real;
          stats.blocks = blocks.size();
        }
      map.unlink();
      return stats;
    }

  if(!records.buckets)
    {
      throw std::logic_error("sorted blocks: records hold neither a file nor buckets");
    }
  stats.out_of_core = true;

  struct piece_t
  {
    bool sentinels;
    size_t n;
    std::vector<SortedBlock> blocks;
  };
  std::optional<T> previous_last;

  auto process = [&](size_t offset, std::span<T> records_in)
  {
    std::span<const T> run(records_in.data(), records_in.size());
    if(sorted_is_sentinel(run.front(), sentinel))
      {
        for(const T& record : run)
          {
            if(!sorted_is_sentinel(record, sentinel))
              {
                throw std::logic_error(std::format("sorted blocks: sentinel bucket at record {:d} holds a real record", offset));
              }
          }
        return piece_t{true, run.size(), {}};
      }
    FlashSort<T>().sort(records_in.data(), records_in.data() + records_in.size());
    if(sorted_is_sentinel(run.back(), sentinel))
      {
        throw std::logic_error(std::format("sorted blocks: bucket at record {:d} holds a sentinel", offset));
      }
    // The first block is counted again on commit, with the previous run's
    // last record before it.
    return piece_t{false, run.size(), sorted_count_run<T>(run, offset, nullptr, count)};
  };

  auto commit = [&](size_t offset, piece_t piece, std::span<T> records_in)
  {
    if(piece.sentinels)
      {
        stats.sentinels += piece.n;
        return;
      }
    std::span<const T> run(records_in.data(), records_in.size());
    const T *before = previous_last ? &*previous_last : nullptr;
    SortedBlock& first = piece.blocks.front();
    first.count = count(first.offset, run.subspan(0, first.n), before);
    stats.total = sorted_assign_bases(piece.blocks, stats.total);
    sorted_emit_run<T>(run, before, piece.blocks, emit);
    previous_last = run.back();
    stats.records += piece.n;
    stats.blocks += piece.blocks.size();
    (void) offset;
  };

  for_each_bucket<T>(std::move(*records.buckets), 0, process, commit, stats.buckets, 0);
  return stats;
}

// Records sorted by key and counted block by block, for an emit phase once
// the total is known. At or under the cutoff the records are the populated
// file, sorted in place; above it, the buckets sorted and written in key
// order to one file, read back block by block.
template <class T>
class CountedRecords
{
  MemoryMap<T> map;
  bool in_core;

public:

  std::vector<SortedBlock> blocks;
  SortedBlocksStats stats;

  CountedRecords(MemoryMap<T>&& map_in, bool in_core_in, std::vector<SortedBlock>&& blocks_in, SortedBlocksStats stats_in)
    : map(std::move(map_in)),
      in_core(in_core_in),
      blocks(std::move(blocks_in)),
      stats(stats_in)
  {
  }

  size_t total() const
  {
    return stats.total;
  }

  // Calls emit on every block once, in parallel, checking each emit's
  // count, then unlinks the sorted records' file.
  template <class Emit>
  void emit(Emit emit_func)
  {
    if(in_core)
      {
        if(!blocks.empty())
          {
            std::span<const T> run(map.begin(), map.size());
            sorted_emit_run<T>(run, nullptr, blocks, emit_func);
          }
        map.unlink();
        return;
      }

    std::string path = map.filename();
    int fd = ::open(path.c_str(), O_RDONLY);
    if(fd < 0)
      {
        throw std::system_error(errno, std::generic_category(), "sorted blocks: open " + path);
      }
    try
      {
        bucket_parallel_for(blocks.size(), [&](size_t k)
        {
          const SortedBlock& block = blocks[k];
          // the block with the record before it
          size_t lead = (block.offset > 0) ? 1 : 0;
          std::vector<T> buffer(block.n + lead);
          pread_bytes(fd, buffer.data(), buffer.size() * sizeof(T), (block.offset - lead) * sizeof(T));
          std::span<const T> run(buffer.data() + lead, block.n);
          const T *before = lead ? buffer.data() : nullptr;
          size_t emitted = emit_func(block.offset, run, before, block.base);
          if(emitted != block.count)
            {
              throw std::logic_error(std::format("sorted blocks: block at record {:d} emitted {:d}, but counted {:d}", block.offset, emitted, block.count));
            }
        });
      }
    catch(...)
      {
        ::close(fd);
        throw;
      }
    if(::close(fd) != 0)
      {
        throw std::system_error(errno, std::generic_category(), "sorted blocks: close " + path);
      }
    map.unlink();
  }
};

// Sorts records by key and counts every block, as described above, for an
// emit phase once the total is known. Out of core the sorted records are
// written to sorted_path.
template <class T, class Count>
CountedRecords<T> sorted_count(PopulatedRecords<T>&& records, const std::string& sorted_path, Count count)
{
  SortedBlocksStats stats;

  if(records.map)
    {
      MemoryMap<T> map = std::move(*records.map);
      std::vector<SortedBlock> blocks;
      size_t n = map.size();
      if(n > 0)
        {
          FlashSort<T>().sort(map.begin(), map.end());
          blocks = sorted_count_run<T>(std::span<const T>(map.begin(), n), 0, nullptr, count);
        }
      stats.total = sorted_assign_bases(blocks, 0);
      stats.records = n;
      stats.blocks = blocks.size();
      return CountedRecords<T>(std::move(map), true, std::move(blocks), stats);
    }

  if(!records.buckets)
    {
      throw std::logic_error("sorted blocks: records hold neither a file nor buckets");
    }
  stats.out_of_core = true;

  size_t n = records.buckets->total();
  std::vector<SortedBlock> blocks;
  std::mutex blocks_mutex;
  std::optional<T> previous_last;

  BucketObserver<T> observer;
  observer.sorted = [&](size_t offset, std::span<const T> run)
  {
    std::vector<SortedBlock> piece = sorted_count_run<T>(run, offset, nullptr, count);
    std::lock_guard<std::mutex> lock(blocks_mutex);
    blocks.insert(blocks.end(), piece.begin(), piece.end());
  };
  // The first block of each run is counted again in key order, with the
  // previous run's last record before it.
  observer.committed = [&](size_t offset, std::span<const T> run)
  {
    const T *before = previous_last ? &*previous_last : nullptr;
    size_t first_n = std::min(run.size(), sorted_block_records(sizeof(T)));
    size_t first_count = count(offset, run.subspan(0, first_n), before);
    {
      std::lock_guard<std::mutex> lock(blocks_mutex);
      auto found = std::find_if(blocks.begin(), blocks.end(), [&](const SortedBlock& block) {return block.offset == offset;});
      if(found == blocks.end())
        {
          throw std::logic_error(std::format("sorted blocks: no block counted at record {:d}", offset));
        }
      found->count = first_count;
    }
    previous_last = run.back();
  };

  MemoryMap<T> map(sorted_path, n);
  map.munmap();
  bucket_sort_write_back(std::move(*records.buckets), sorted_path, stats.buckets, &observer);

  std::sort(blocks.begin(), blocks.end(), [](const SortedBlock& a, const SortedBlock& b) {return a.offset < b.offset;});
  size_t expected = 0;
  for(const SortedBlock& block : blocks)
    {
      if(block.offset != expected)
        {
          throw std::logic_error(std::format("sorted blocks: blocks of {:s} reach record {:d}, then start again at {:d}", sorted_path, expected, block.offset));
        }
      expected += block.n;
    }
  if(expected != n)
    {
      throw std::logic_error(std::format("sorted blocks: blocks of {:s} cover {:d} of {:d} records", sorted_path, expected, n));
    }

  stats.total = sorted_assign_bases(blocks, 0);
  stats.records = n;
  stats.blocks = blocks.size();
  return CountedRecords<T>(std::move(map), false, std::move(blocks), stats);
}

// Collects count records, written in runs at given offsets from many
// threads at once, into records for sorted_emit or sorted_count: at or
// under the cutoff a file of them at path, each at its offset; above it
// bucket files by the digit from digit_bit, as bucket_populate makes them.
template <class T>
class RecordSink
{
  size_t count;
  std::optional<MemoryMap<T>> map;
  std::optional<BucketSpec<T>> spec;
  std::unique_ptr<BucketAppender<T>> appender;
  std::mutex writers_mutex;
  std::vector<std::unique_ptr<typename BucketAppender<T>::Writer>> writers;
  std::vector<typename BucketAppender<T>::Writer *> free_writers;
  std::string path;

public:

  RecordSink(const std::string& path_in, size_t count_in, int digit_bit)
    : count(count_in),
      path(path_in)
  {
    if((count == 0) || !bucket_sort_out_of_core(count * sizeof(T)))
      {
        map.emplace(path, count);
        return;
      }
    spec = make_bucket_spec<T>(count, bucket_first_digit_bit<T>(digit_bit), T(), std::nullopt);
    size_t slash = path.find_last_of('/');
    std::string dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
    std::string prefix = ((slash == std::string::npos) ? path : path.substr(slash + 1)) + ".bucket";
    size_t threads = std::max(size_t(std::thread::hardware_concurrency()), size_t(1));
    appender = std::make_unique<BucketAppender<T>>(dir, prefix, *spec, threads, bucket_sort_memory_bytes() / 2);
  }

  RecordSink(const RecordSink&) = delete;
  RecordSink& operator=(const RecordSink&) = delete;

  // Puts records at offsets offset to offset + records.size(). Runs put
  // from different threads must not overlap.
  void put(size_t offset, std::span<const T> records)
  {
    if(offset + records.size() > count)
      {
        throw std::logic_error(std::format("RecordSink: records {:d} to {:d} of {:s} past its {:d}", offset, offset + records.size(), path, count));
      }
    if(map)
      {
        std::copy(records.begin(), records.end(), map->begin() + std::ptrdiff_t(offset));
        return;
      }

    typename BucketAppender<T>::Writer *writer = nullptr;
    {
      std::lock_guard<std::mutex> lock(writers_mutex);
      if(free_writers.empty())
        {
          writers.push_back(std::make_unique<typename BucketAppender<T>::Writer>(*appender));
          writer = writers.back().get();
        }
      else
        {
          writer = free_writers.back();
          free_writers.pop_back();
        }
    }
    try
      {
        for(size_t i = 0; i < records.size(); ++i)
          {
            size_t bucket = spec->bucket_of(records[i]);
            if(bucket == BucketSpec<T>::invalid_bucket)
              {
                throw std::logic_error(std::format("RecordSink: record {:d} of {:s} differs from its bucket's prefix before bit {:d}", offset + i, path, spec->digit_bit));
              }
            writer->append(bucket, records[i]);
          }
      }
    catch(...)
      {
        std::lock_guard<std::mutex> lock(writers_mutex);
        free_writers.push_back(writer);
        throw;
      }
    std::lock_guard<std::mutex> lock(writers_mutex);
    free_writers.push_back(writer);
  }

  // The records put, for sorted_emit or sorted_count. Every one of count
  // must have been put exactly once.
  PopulatedRecords<T> finish()
  {
    if(map)
      {
        PopulatedRecords<T> populated{std::move(*map), std::nullopt, false, BucketSortStats()};
        map.reset();
        return populated;
      }
    for(auto& writer : writers)
      {
        writer->flush_all();
      }
    BucketFiles<T> files = appender->finish();
    if(files.total() != count)
      {
        throw std::logic_error(std::format("RecordSink: {:d} records put into buckets of {:s}, expected {:d}", files.total(), path, count));
      }
    return PopulatedRecords<T>{std::nullopt, std::move(files), false, BucketSortStats()};
  }
};

#endif
