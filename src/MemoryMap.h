// MemoryMap.h

#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <ctype.h>

#include <string>

template<class T>
class MemoryMap
{
 private:
  std::string _filename;
  size_t _size;
  size_t _length;

  void *_mapped;

 public:

  MemoryMap(std::string, size_t);
  MemoryMap(const MemoryMap&) = delete;
  MemoryMap(MemoryMap&&);
  ~MemoryMap();

  T& operator[](int);

  T *begin();
  T *end();
  void mmap();
  void munmap();
  size_t size() const;
};

#endif
