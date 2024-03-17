// merge.cpp

#include "merge.h"

#include <cassert>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

#include "MemoryMap.h"

template <class T>
MemoryMap<T> merge(std::string output_filename, const std::vector<MemoryMap<T>>& inputs, bool filter_duplicates)
{
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

  MemoryMap<T> output(output_filename, max_size);
  size_t output_index = 0;

  size_t last_pair = 0; // can't match because it would have been filtered
  while(merge_queue.size())
    {
      merge_entry merge_next = merge_queue.top();
      merge_queue.pop();

      size_t merge_pair = std::get<0>(merge_next);
      assert(merge_pair >= last_pair);

      if((!filter_duplicates) || (merge_pair > last_pair))
        {
          output[output_index++] = merge_pair;
          last_pair = merge_pair;
        }

      const MemoryMap<T> *merge_map = std::get<1>(merge_next);
      size_t merge_index = std::get<2>(merge_next);
      if(merge_index + 1 < merge_map->size())
        {
          merge_queue.emplace((*merge_map)[merge_index + 1], merge_map, merge_index + 1);
        }
    }

  output.truncate(output_index);
  return output;
}

// template instantiations

#define INSTANTIATE(T) template class MemoryMap<T> merge(std::string, const std::vector<MemoryMap<T>>&, bool);

INSTANTIATE(long long unsigned int);
INSTANTIATE(long unsigned int);
