// merge.h

#ifndef MERGE_H
#define MERGE_H

#include <string>
#include <vector>

#include "MemoryMap.h"

// like usual merge sort, but has an option to filter duplicates

template <class T>
MemoryMap<T> merge(std::string, std::vector<MemoryMap<T>>&, bool);

#endif
