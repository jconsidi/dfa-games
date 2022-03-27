// MemoryMap.h

#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <ctype.h>

#include <string>

template<class T>
class MemoryMap
{
 private:
  size_t _size;
  size_t _length;

  void *_mapped;

  void mmap(int, int);
  void munmap();

public:

  MemoryMap(size_t);
  MemoryMap(std::string, size_t);
  MemoryMap(const MemoryMap&) = delete;
  MemoryMap(MemoryMap&&);
  ~MemoryMap();

  T& operator[](size_t);

  T *begin();
  T *end();
  size_t length() const;
  size_t size() const;
};

#endif
