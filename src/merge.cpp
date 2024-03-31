// merge.cpp

#include "merge.h"

#include <algorithm>
#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <numeric>
#include <queue>
#include <string>
#include <vector>
#include <unistd.h>

#include "MemoryMap.h"
#include "parallel.h"
#include "profile.h"
#include "utils.h"

template <class T>
MemoryMap<T> merge_pair(std::string output_filename, const MemoryMap<T>& input_a, const MemoryMap<T>& input_b, bool filter_duplicates)
{
  std::cerr << "  merging " << input_a.filename() << " (" << input_a.size() << ") and " << input_b.filename() << " (" << input_b.size() << ") into " << output_filename << std::endl;

  // open output file

  int fildes = open(output_filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if(fildes == -1)
    {
      throw std::runtime_error("open() failed");
    }

  // setup buffer

  const size_t output_buffer_bytes_max = size_t(1) << 30;
  static_assert(output_buffer_bytes_max % sizeof(T) == 0);
  const size_t output_buffer_elements_max = output_buffer_bytes_max / sizeof(T);
  const size_t output_buffer_capacity = std::min(output_buffer_elements_max, input_a.size() + input_b.size());
  assert(output_buffer_capacity >= 2);

  std::vector<T> output_buffer;
  output_buffer.reserve(output_buffer_capacity);

  size_t output_elements = 0;
  auto write_helper = [&]()
  {
#ifdef PARANOIA
    std::cerr << "  checking buffer" << std::endl;
    assert(TRY_PARALLEL_2(std::is_sorted, output_buffer.begin(), output_buffer.end()));
#endif

    std::cerr << "  writing buffer size " << output_buffer.size() << std::endl;
    write_buffer(fildes, output_buffer.data(), output_buffer.size());
    output_elements += output_buffer.size();
  };

  // merge prefixes in the output buffer until

  size_t offset_a = 0;
  size_t offset_b = 0;

  auto handle_split = [&](size_t next_offset_a, size_t next_offset_b)
  {
    assert(next_offset_a >= offset_a);
    assert(next_offset_b >= offset_b);

    // resize buffer to match

    size_t next_size = (next_offset_a - offset_a) + (next_offset_b - offset_b);
    assert(0 < next_size);
    assert(next_size <= output_buffer_capacity);

    output_buffer.resize(next_size);

    // merge sort

    if(filter_duplicates)
      {
        auto end = TRY_PARALLEL_5(std::set_union,
                                  input_a.begin() + offset_a,
                                  input_a.begin() + next_offset_a,
                                  input_b.begin() + offset_b,
                                  input_b.begin() + next_offset_b,
                                  output_buffer.begin());

        next_size = end - output_buffer.begin();
        if(next_size != output_buffer.size())
          {
            output_buffer.resize(next_size);
          }
        }
    else
      {
        auto end = TRY_PARALLEL_5(std::merge,
                                  input_a.begin() + offset_a,
                                  input_a.begin() + next_offset_a,
                                  input_b.begin() + offset_b,
                                  input_b.begin() + next_offset_b,
                                  output_buffer.begin());
        assert(end == output_buffer.end());
      }

    write_helper();

    offset_a = next_offset_a;
    offset_b = next_offset_b;
  };

  auto get_split_a = [&](const T& split_value)
  {
    // log cost
    return std::lower_bound(input_a.begin() + offset_a,
                            input_a.end(),
                            split_value) - input_a.begin();
  };

  auto get_split_b = [&](const T& split_value)
  {
    // log cost
    return std::lower_bound(input_b.begin() + offset_b,
                            input_b.end(),
                            split_value) - input_b.begin();
  };

  auto calculate_split_cost = [&](const T& split_value)
  {
    // log cost

    size_t split_a = get_split_a(split_value);
    size_t split_b = get_split_b(split_value);

    return (split_a - offset_a) + (split_b - offset_b);
  };

  auto consider_split_value = [&](const T& split_value)
  {
    // log cost
    return calculate_split_cost(split_value) <= output_buffer_capacity;
  };

  auto pick_split_value = [&](const MemoryMap<T>& input, size_t offset)
  {
    assert(offset < input.size());

    size_t next_offset = std::partition_point(input.begin() + offset,
                                              input.end(),
                                              consider_split_value) - input.begin();
    if(next_offset > offset)
      {
        --next_offset;
      }

    return input[next_offset];
  };

  while((offset_a < input_a.size()) &&
        (offset_b < input_b.size()))
    {
      if((input_a.size() - offset_a) + (input_b.size() - offset_b) <= output_buffer_capacity)
        {
          // do the rest all at once
          handle_split(input_a.size(), input_b.size());
          break;
        }

      const T& split_value_a = pick_split_value(input_a, offset_a);
      const T& split_value_b = pick_split_value(input_b, offset_b);

      size_t split_cost_a = calculate_split_cost(split_value_a);
      size_t split_cost_b = calculate_split_cost(split_value_b);

      if(std::max(split_cost_a, split_cost_b) <= output_buffer_capacity)
        {
          // both splits work, so go with the one that does more work
          const T& split_value_best = std::max(split_value_a, split_value_b);
          handle_split(get_split_a(split_value_best),
                       get_split_b(split_value_best));
          continue;
        }

      if(split_cost_a <= output_buffer_capacity)
        {
          // only a-side split works
          handle_split(get_split_a(split_value_a),
                       get_split_b(split_value_a));
          continue;
        }

      if(split_cost_b <= output_buffer_capacity)
        {
          // only b-side split works
          handle_split(get_split_a(split_value_b),
                       get_split_b(split_value_b));
          continue;
        }

      // if filtering duplicates, second lowest remaining value should
      // always work because cost is at most two.

      // if not filtering duplicates, minimum non-zero cost is the
      // number of ties on lowest value. will only fail if there is a
      // tie bigger than the max buffer capacity. but that should not
      // happen with current use cases.

      assert(split_value_a == input_a[offset_a]);
      assert(split_value_b == input_b[offset_b]);

      throw std::logic_error("merge split failed");
    }

  auto copy_remainder = [&](const MemoryMap<T>& input_remaining,
                            size_t offset_remaining)
  {
    assert(offset_remaining <= input_remaining.size());
    if(offset_remaining >= input_remaining.size())
      {
        return;
      }

    output_buffer.resize(input_remaining.size() - offset_remaining);

    auto copy_end = TRY_PARALLEL_3(std::copy,
                                   input_remaining.begin() + offset_remaining,
                                   input_remaining.end(),
                                   output_buffer.begin());
    assert(copy_end == output_buffer.end());

    write_helper();
  };

  copy_remainder(input_a, offset_a);
  copy_remainder(input_b, offset_b);

  if(close(fildes))
    {
      perror("close");
      throw std::runtime_error("close() failed");
    }

  // implicit truncate
  return MemoryMap<T>(output_filename, output_elements);
}

template <class T>
MemoryMap<T> merge(std::string output_filename, std::vector<MemoryMap<T>>& inputs, bool filter_duplicates)
{
  assert(inputs.size() > 0);

  // paranoid input check

  for(int i = 0; i < inputs.size(); ++i)
    {
      // assuming this file
      assert(inputs[i].filename() == get_temp_filename(i));
    }

#ifdef PARANOIA
  for(const MemoryMap<T>& input : inputs)
    {
      std::cerr << "  checking input" << std::endl;
      assert(TRY_PARALLEL_2(std::is_sorted, input.begin(), input.end()));
    }
#endif

  // singleton case

  if(inputs.size() == 1)
    {
      inputs[0].rename(output_filename);
      return MemoryMap<T>(output_filename);
    }

  // merge items in pairs

  std::cerr << "  merging " << inputs.size() << " inputs" << std::endl;

  std::vector<MemoryMap<T> *> combined_entries;
  for(int i = 0; i < inputs.size(); ++i)
    {
      assert(inputs[i].filename() == get_temp_filename(combined_entries.size()));
      combined_entries.push_back(&inputs[i]);
    }

  std::vector<MemoryMap<T>> merged_entries;
  merged_entries.reserve(inputs.size() - 1);
  for(int i = 0; i < inputs.size() - 1; ++i)
    {
      assert(2 * i + 1 < combined_entries.size());
      merged_entries.push_back(merge_pair(get_temp_filename(combined_entries.size()),
                                          *combined_entries[2 * i],
                                          *combined_entries[2 * i + 1],
                                          filter_duplicates));
      assert(merged_entries.size() == i + 1);

      combined_entries[2 * i]->unlink();
      combined_entries[2 * i + 1]->unlink();

      combined_entries.push_back(&merged_entries[i]);
    }
  assert(merged_entries.size() == inputs.size() - 1);
  assert(combined_entries.size() == inputs.size() * 2 - 1);

  merged_entries.back().rename(output_filename);
  return MemoryMap<T>(output_filename);
}

template <class T>
MemoryMap<T> build_merge_sort(std::string filename_in, size_t size_in, std::function<T(size_t)> populate_func)
{
  Profile profile("build_merge_sort");

  profile.tic("init");

  const size_t buffer_bytes_max = 1 << 30;
  static_assert(buffer_bytes_max % sizeof(T) == 0);
  size_t buffer_elements_max = buffer_bytes_max / sizeof(T);
  size_t buffer_capacity = std::min(buffer_elements_max, size_in);
  std::vector<T> buffer;
  buffer.reserve(buffer_capacity);

  profile.tic("iota");
  std::vector<size_t> iota(buffer_capacity);
  std::iota(iota.begin(), iota.end(), 0);

  std::vector<MemoryMap<T>> files_temp;
  size_t files_capacity = (size_in + (buffer_capacity - 1)) / buffer_capacity;
  files_temp.reserve(files_capacity);

  for(size_t i = 0; i < files_capacity; ++i)
    {
      profile.tic("chunk resize");

      size_t offset_begin = i * buffer_capacity;
      size_t offset_end = std::min(offset_begin + buffer_capacity, size_in);
      buffer.resize(offset_end - offset_begin);

      profile.tic("chunk populate");

      TRY_PARALLEL_4(std::transform,
                     iota.begin(),
                     iota.begin() + (offset_end - offset_begin),
                     buffer.begin(),
                     [&](size_t index)
                     {
                       return populate_func(offset_begin + index);
                     });

      profile.tic("chunk sort");

      TRY_PARALLEL_2(std::sort,
                     buffer.begin(),
                     buffer.end());

      files_temp.emplace_back(get_temp_filename(i), buffer);
    }

  profile.tic("merge");

  MemoryMap<T> output = merge(filename_in, files_temp, false);
  assert(output.size() == size_in);

  profile.tic("done");
  return output;
}

// template instantiations

#include "BinaryDFA.h"

#define INSTANTIATE(T) template MemoryMap<T> build_merge_sort(std::string, size_t, std::function<T(size_t)>);

INSTANTIATE(BinaryDFATransitionsHashPlusIndex);
INSTANTIATE(dfa_state_pair);
INSTANTIATE(long long unsigned int);
INSTANTIATE(long unsigned int);
