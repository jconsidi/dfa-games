// merge.cpp

#include "merge.h"

#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <queue>
#include <string>
#include <vector>
#include <unistd.h>

#include "MemoryMap.h"
#include "utils.h"

template <class T>
MemoryMap<T> merge(std::string output_filename, std::vector<MemoryMap<T>>& inputs, bool filter_duplicates)
{
  assert(inputs.size() > 0);

  if(inputs.size() == 1)
    {
      inputs[0].rename(output_filename);
      return MemoryMap<T>(output_filename);
    }

  std::cerr << "  merging " << inputs.size() << " inputs" << std::endl;

  // setup priority queue

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
    write_buffer(fildes, output_buffer.data(), output_buffer.size());
    output_elements += output_buffer.size();
  };

  // merge inputs and write to file

  T last_element = {0};
  bool last_element_set = false;
  while(merge_queue.size())
    {
      merge_entry merge_next = merge_queue.top();
      merge_queue.pop();

      T merge_element = std::get<0>(merge_next);
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
  return MemoryMap<T>(output_filename, output_elements);
}

// template instantiations

#include "BinaryDFA.h"

#define INSTANTIATE(T) template class MemoryMap<T> merge(std::string, std::vector<MemoryMap<T>>&, bool);

INSTANTIATE(BinaryDFATransitionsHashPlusIndex);
INSTANTIATE(long long unsigned int);
INSTANTIATE(long unsigned int);
