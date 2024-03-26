// merge.cpp

#include "merge.h"

#include <algorithm>
#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <queue>
#include <string>
#include <vector>
#include <unistd.h>

#include "MemoryMap.h"
#include "parallel.h"
#include "profile.h"
#include "utils.h"

template <class T>
MemoryMap<T> merge(std::string output_filename, std::vector<MemoryMap<T>>& inputs, bool filter_duplicates)
{
  assert(inputs.size() > 0);

  // paranoid input check

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

  // setup priority queue

  std::cerr << "  merging " << inputs.size() << " inputs" << std::endl;

  typedef std::tuple<T, const MemoryMap<T> *, size_t> merge_entry;
  auto merge_compare = [](const merge_entry& a, const merge_entry& b)
  {
    return std::get<0>(a) > std::get<0>(b);
  };
  std::priority_queue<merge_entry, std::vector<merge_entry>, decltype(merge_compare)> merge_queue(merge_compare);

  size_t max_size = 0;
  for(const MemoryMap<T>& input : inputs)
    {
      max_size += input.size();
      merge_queue.emplace(input[0], &input, 0);
    }

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
  const size_t output_buffer_capacity = std::min(output_buffer_elements_max, max_size);

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

  // merge inputs and write to file

  T last_element = {};
  bool last_element_set = false;
  while(merge_queue.size())
    {
      merge_entry merge_next = merge_queue.top();
      merge_queue.pop();

      T merge_element = std::get<0>(merge_next);
      if(last_element_set)
        {
          assert(!(merge_element < last_element));
        }
      if((!filter_duplicates) || (!last_element_set) || (merge_element > last_element))
        {
          output_buffer.push_back(merge_element);
          last_element = merge_element;
          last_element_set = true;

          if(output_buffer.size() == output_buffer_capacity)
            {
              write_helper();
              output_buffer.resize(0);
            }
        }

      const MemoryMap<T> *merge_map = std::get<1>(merge_next);
      size_t merge_index = std::get<2>(merge_next);
      if(merge_index + 1 < merge_map->size())
        {
          merge_queue.emplace((*merge_map)[merge_index + 1], merge_map, merge_index + 1);
        }
    }

  if(output_buffer.size())
    {
      write_helper();
    }

  if(close(fildes))
    {
      perror("close");
      throw std::runtime_error("close() failed");
    }

  // implicit truncate
  MemoryMap<T> output(output_filename, output_elements);

#ifdef PARANOIA
  assert(TRY_PARALLEL_2(std::is_sorted, output.begin(), output.end()));
#endif

  return output;
}

template <class T>
void merge_sort(MemoryMap<T>& data)
{
  Profile profile("merge_sort");

  size_t data_size = data.size();

  profile.tic("init");

  const size_t buffer_bytes_max = 1 << 30;
  static_assert(buffer_bytes_max % sizeof(T) == 0);
  size_t buffer_elements_max = buffer_bytes_max / sizeof(T);
  size_t buffer_capacity = std::min(buffer_elements_max, data.size());
  std::vector<T> buffer;
  buffer.reserve(buffer_capacity);

  std::vector<MemoryMap<T>> files_temp;
  size_t files_capacity = (data.size() + (buffer_capacity - 1)) / buffer_capacity;
  files_temp.reserve(files_capacity);

  for(size_t i = 0; i < files_capacity; ++i)
    {
      profile.tic("chunk");

      size_t offset_begin = i * buffer_capacity;
      size_t offset_end = std::min(offset_begin + buffer_capacity,
                                   data.size());
      buffer.resize(offset_end - offset_begin);

      TRY_PARALLEL_3(std::copy,
                     data.begin() + offset_begin,
                     data.begin() + offset_end,
                     buffer.begin());

      TRY_PARALLEL_2(std::sort,
                     buffer.begin(),
                     buffer.end());

      files_temp.emplace_back(std::format("scratch/binarydfa/temp_{:03d}", i), buffer);
    }

  data.munmap();

  profile.tic("merge");

  data = merge(data.filename(),
               files_temp,
               false);
  assert(data.size() == data_size);
}

// template instantiations

#include "BinaryDFA.h"

#define INSTANTIATE(T) template void merge_sort(MemoryMap<T>& data);

INSTANTIATE(BinaryDFATransitionsHashPlusIndex);
INSTANTIATE(dfa_state_pair);
INSTANTIATE(long long unsigned int);
INSTANTIATE(long unsigned int);
