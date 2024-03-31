// merge.h

#ifndef MERGE_H
#define MERGE_H

#include <functional>
#include <string>
#include <vector>

#include "MemoryMap.h"

// like usual merge sort, but has an option to filter duplicates

template <class T>
MemoryMap<T> merge(std::string, std::vector<MemoryMap<T>>&, bool);

template <class T>
MemoryMap<T> build_merge_sort(std::string, size_t, std::function<T(size_t)>);

#endif
